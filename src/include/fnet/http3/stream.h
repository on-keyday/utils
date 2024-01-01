/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../quic/stream/impl/stream.h"
#include "../quic/stream/impl/recv_stream.h"
#include "frame.h"
#include "../util/qpack/qpack.h"
#include "error.h"
#include "../quic/transport_error.h"

namespace futils {
    namespace fnet::http3::stream {
        enum class SettingID {
            qpack_max_table_capacity = 0x01,
            max_field_section_size = 0x06,
            qpack_blocked_streams = 0x07,
        };

        struct KnownSettings {
            std::uint64_t max_field_section_size = 0;
        };

        template <class Config>
        struct ControlStream {
            using QuicRecvStream = quic::stream::impl::RecvUniStream<typename Config::quic_config>;
            using QuicSendStream = quic::stream::impl::SendUniStream<typename Config::quic_config>;
            std::shared_ptr<QuicSendStream> send_control;
            std::shared_ptr<QuicRecvStream> recv_control;

           private:
            bool write_push_frame(frame::Type type, std::uint32_t push_id) {
                QuicSendStream* q = send_control.get();
                if (q->id().dir() == quic::stream::Origin::client) {
                    return false;
                }
                frame::FrameHeaderArea id_area;
                binary::writer w{id_area};
                quic::varint::write(w, push_id);
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, type, quic::varint::len(push_id));
                auto err = q->add_multi_data(false, true, header, w.written());
                if (err != quic::IOResult::ok) {
                    return false;
                }
                return true;
            }

           public:
            bool write_max_push_id(std::uint32_t push_id) {
                return write_push_frame(frame::Type::MAX_PUSH_ID, push_id);
            }

            bool write_cancel_push(std::uint32_t push_id) {
                return write_push_frame(frame::Type::CANCEL_PUSH, push_id);
            }

            bool write_settings(auto&& write) {
                QuicSendStream* q = send_control.get();
                flex_storage buf;
                write([&](std::uint64_t id, std::uint64_t data) {
                    frame::FrameHeaderArea area;
                    frame::Setting setting;
                    setting.id = id;
                    setting.value = data;
                    binary::writer w{area};
                    if (!setting.render(w)) {
                        return false;
                    }
                    buf.append(w.written());
                    return true;
                });
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::SETTINGS, buf.size());
                auto err = q->add_multi_data(false, true, header, buf);
                if (err != quic::IOResult::ok) {
                    return false;
                }
                return true;
            }
        };

        template <class Config>
        struct Connection {
            qpack::Context<typename Config::qpack_config> table;
            using QuicRecvStream = quic::stream::impl::RecvUniStream<typename Config::quic_config>;
            using QuicSendStream = quic::stream::impl::SendUniStream<typename Config::quic_config>;
            std::shared_ptr<QuicRecvStream> recv_decoder;
            std::shared_ptr<QuicRecvStream> recv_encoder;
            std::shared_ptr<QuicSendStream> send_decoder;
            std::shared_ptr<QuicSendStream> send_encoder;

            using Lock = typename Config::conn_lock;
            Lock locker;
            quic::AppError app_err;
            std::uint64_t peer_max_push_id = 0;

            auto lock() {
                locker.lock();
                return helper::defer([&] {
                    locker.unlock();
                });
            }

            bool read_field_section(quic::stream::StreamID id, binary::reader& r, auto&& read) {
                auto locked = lock();
                auto err = table.read_header(id, r, read);
                if (err != qpack::QpackError::none) {
                    if (err == qpack::QpackError::input_length) {
                        return false;
                    }
                    app_err.set_error_code(Error::H3_INTERNAL_ERROR);
                    return false;
                }
                return true;
            }

            template <class String>
            bool write_field_section(quic::stream::StreamID id, binary::expand_writer<String>& w, auto&& write) {
                auto locked = lock();
                auto err = table.write_header(id, w, [&](auto&& add_entry, auto&& add_field) {
                    auto add_entry_wrap = qpack::http3_entry_validate_wrapper(add_entry);
                    auto add_field_wrap = qpack::http3_field_validate_wrapper(add_field);
                    write(add_entry, add_field);
                });
                if (err != qpack::QpackError::none) {
                    if (err == qpack::QpackError::output_length) {
                        return false;
                    }
                    app_err.set_error_code(Error::H3_INTERNAL_ERROR);
                    return false;
                }
                return true;
            }

            void set_error(Error err) {
                auto locked = lock();
                app_err.set_error_code(err);
            }
        };

        template <class Config>
        struct RequestStream {
            using QuicStream = quic::stream::impl::BidiStream<typename Config::quic_config>;
            using Reader = quic::stream::impl::RecvSorter<typename Config::reader_lock>;
            std::shared_ptr<QuicStream> stream;
            std::shared_ptr<Connection<Config>> conn;
            std::shared_ptr<Reader> reader;
            flex_storage read_buf;
            using String = typename Config::qpack_config::string;

            bool write_header(auto&& write) {
                QuicStream* q = stream.get();
                if (q->sender.is_fin()) {
                    return false;
                }
                binary::expand_writer<String> w;
                if (!conn->write_field_section(stream->sender.id(), w, write)) {
                    return false;
                }
                auto section = w.written();
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::HEADER, section.size());
                auto res = q->sender.add_multi_data(false, true, header, section);
                if (res.result != quic::IOResult::ok) {
                    return false;
                }
                return true;
            }

           private:
            bool append_to_read_buf() {
                QuicStream* q = stream.get();
                if (q->receiver.is_closed()) {
                    return false;
                }
                Reader* re = reader.get();
                bool one = false;
                while (true) {
                    auto [data, eof] = re->read_direct();
                    if (data.size() == 0 && !eof) {
                        return one;
                    }
                    read_buf.append(data);
                    one = true;
                    if (eof) {
                        q->receiver.user_read_full();
                        break;
                    }
                }
                return true;
            }

           public:
            bool read_header(auto&& read) {
                if (read_buf.size() == 0) {
                    if (!append_to_read_buf()) {
                        return false;
                    }
                }
                binary::reader r{read_buf};
                frame::Headers hdrs;
                while (!hdrs.parse(r)) {
                    if (read_buf.size() && hdrs.type != std::uint64_t(frame::Type::HEADER)) {
                        return false;
                    }
                    if (!append_to_read_buf()) {
                        return false;
                    }
                }
                auto offset = r.offset();
                r.reset(hdrs.encoded_field);
                auto err = conn->read_field_section(stream->receiver.id(), r, read);
                if (!err) {
                    return false;
                }
                read_buf.shift_front(offset);
                return true;
            }

            bool read(auto&& callback) {
                if (read_buf.size() == 0) {
                    if (!append_to_read_buf()) {
                        return false;
                    }
                }
                binary::reader r{read_buf};
                frame::Data hdrs;
                while (!hdrs.parse(r)) {
                    if (read_buf.size() && hdrs.type != std::uint64_t(frame::Type::DATA)) {
                        return false;
                    }
                    if (!append_to_read_buf()) {
                        return false;
                    }
                }
                callback(hdrs.data);
                read_buf.shift_front(r.offset());
                return true;
            }

            bool write(view::rvec data) {
                QuicStream* q = stream.get();
                if (q->sender.is_fin()) {
                    return false;
                }
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::DATA, data.size());
                auto res = q->sender.add_data(header, false, true);
                if (res != quic::IOResult::ok) {
                    return false;
                }
                res = q->sender.add_data(data, false, true);
                if (res != quic::IOResult::ok) {
                    return false;
                }
                return true;
            }

            bool write_push_promise(std::uint64_t push_id, auto&& write) {
                QuicStream* q = stream.get();
                if (q->id().dir() == quic::stream::Origin::client) {
                    return false;
                }
                if (q->sender.is_fin()) {
                    return false;
                }
                frame::FrameHeaderArea id_area;
                binary::writer id_w{id_area};
                if (!quic::varint::write(id_w, push_id)) {
                    return false;
                }
                binary::expand_writer<String> w;
                if (!conn->write_field_section(stream->sender.id(), w, write)) {
                    return false;
                }
                auto section = w.written();
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::PUSH_PROMISE, quic::varint::len(push_id) + section.size());
                auto res = q->sender.add_multi_data(false, true, header, id_w.written(), section);
                if (res != quic::IOResult::ok) {
                    return false;
                }
                return true;
            }
        };
    }  // namespace fnet::http3::stream
}  // namespace futils
