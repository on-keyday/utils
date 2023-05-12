/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../quic/stream/impl/stream.h"
#include "frame.h"
#include "../../net_util/qpack/qpack.h"
#include "error.h"
#include "../quic/transport_error.h"

namespace utils {
    namespace fnet::http3::stream {
        template <class Config>
        struct Connection {
            qpack::Context<typename Config::qpack_config> table;
            using QuicRecvStream = quic::stream::impl::RecvUniStream<typename Config::quic_config>;
            using QuicSendStream = quic::stream::impl::SendUniStream<typename Config::quic_config>;
            std::shared_ptr<QuicRecvStream> recv_decoder;
            std::shared_ptr<QuicRecvStream> recv_encoder;
            std::shared_ptr<QuicSendStream> send_decoder;
            std::shared_ptr<QuicSendStream> send_encoder;
            std::shared_ptr<QuicSendStream> send_control;
            std::shared_ptr<QuicRecvStream> recv_control;
            using Lock = typename Config::conn_lock;
            Lock locker;
            quic::AppError app_err;
            std::uint64_t peer_max_push_id = 0;

            auto lock() {
                locker.lock();
                return helpe::defer([&] {
                    locker.unlock();
                });
            }

            template <class String>
            bool write_field_section(quic::stream::StreamID id, io::expand_writer<String>& w, auto&& write) {
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
           private:
            using QuicStream = quic::stream::impl::BidiStream<typename Config::quic_config>;
            std::shared_ptr<QuicStream> stream;
            std::shared_ptr<Connection> conn;
            using String = typename Config::qpack_config::string;

           public:
            bool write_header(auto&& write) {
                QuicStream* q = stream.get();
                if (q->sender.is_fin()) {
                    return false;
                }
                io::expand_writer<String> w;
                if (!conn->write_field_section(stream->sender.id(), w, write)) {
                    return false;
                }
                auto section = w.written();
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::HEADER, section.size());
                auto res = q->sender.add_multi_data(false, true, header, section);
                if (res != quic::IOResult::ok) {
                    return false;
                }
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

            bool push_promise(std::uint64_t push_id, auto&& write) {
                if (push_id >= quic::varint::border(8)) {
                    return false;
                }
                QuicStream* q = stream.get();
                if (q->id().dir() == quic::stream::Direction::client) {
                    return false;
                }
                if (q->sender.is_fin()) {
                    return false;
                }
                frame::FrameHeaderArea id_area;
                io::writer id_w{id_area};
                if (!quic::varint::write(id_w, push_id)) {
                    return false;
                }
                io::expand_writer<String> w;
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
}  // namespace utils
