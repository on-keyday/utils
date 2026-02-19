/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../quic/stream/impl/stream.h"
#include "../quic/stream/impl/recv_stream.h"
#include "frame.h"
#include "../util/qpack/qpack.h"
#include "error_enum.h"
#include "../quic/transport_error.h"
#include "unistream.h"

namespace futils {
    namespace fnet::quic::context {
        template <class TConfig>
        struct Context;
    }
    namespace fnet::http3::stream {
        enum class SettingID {
            qpack_max_table_capacity = 0x01,
            max_field_section_size = 0x06,
            qpack_blocked_streams = 0x07,
        };

        struct KnownSettings {
            std::uint64_t max_field_section_size = 0;
        };

        template <class QuicStreamConfig>
        bool append_to_read_buf(quic::stream::impl::RecvUniStream<QuicStreamConfig>& q, flex_storage& read_buf) {
            using Reader = quic::stream::impl::RecvSorter<typename QuicStreamConfig::recv_stream_lock>;
            Reader* re = q.get_receiver_ctx().get();
            if (q.is_closed()) {
                return false;
            }
            auto eof = re->read_best([&](auto& data) {
                read_buf.append(data);
                return true;
            });
            if (eof) {
                q.user_read_full();
            }
            return true;
        }

        template <class QuicStreamConfig>
        struct ControlStream {
            using QuicRecvStream = quic::stream::impl::RecvUniStream<QuicStreamConfig>;
            using QuicSendStream = quic::stream::impl::SendUniStream<QuicStreamConfig>;
            using Reader = quic::stream::impl::RecvSorter<typename QuicStreamConfig::recv_stream_lock>;
            std::shared_ptr<QuicSendStream> send_control;
            std::shared_ptr<QuicRecvStream> recv_control;
            flex_storage read_buf;

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
                auto err = q->write_ex(false, true, header, w.written());
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
                auto result = q->write_ex(false, true, header, buf);
                if (result.result != quic::IOResult::ok) {
                    return false;
                }
                return true;
            }

            bool read_settings(auto&& read) {
                QuicRecvStream* q = recv_control.get();
                frame::Settings settings;
                binary::reader r{read_buf};
                while (!settings.parse(r)) {
                    if (read_buf.size() && settings.type != std::uint64_t(frame::Type::SETTINGS)) {
                        return false;
                    }
                    if (!append_to_read_buf(*q, read_buf)) {
                        return false;
                    }
                }
                return settings.visit_settings([&](auto&& setting) {
                    read(setting.id, setting.value);
                    return true;
                });
            }
        };

        template <class Config>
        struct Connection {
            qpack::Context<typename Config::qpack_config> table;
            using QuicRecvStream = quic::stream::impl::RecvUniStream<typename Config::quic_config>;
            using QuicSendStream = quic::stream::impl::SendUniStream<typename Config::quic_config>;
            using Reader = quic::stream::impl::RecvSorter<typename Config::quic_config::recv_stream_lock>;
            std::shared_ptr<QuicRecvStream> recv_decoder;
            std::shared_ptr<QuicRecvStream> recv_encoder;

            std::shared_ptr<QuicSendStream> send_decoder;
            std::shared_ptr<QuicSendStream> send_encoder;
            ControlStream<typename Config::quic_config> ctrl;

            bool write_uni_stream_headers_and_settings(auto&& write_settings) {
                if (!ctrl.send_control || !send_decoder || !send_encoder) {
                    return false;
                }
                unistream::UniStreamHeaderArea area;
                auto hdr = unistream::get_header(area, unistream::Type::CONTROL);
                ctrl.send_control->write_ex(false, true, hdr);
                hdr = unistream::get_header(area, unistream::Type::QPACK_ENCODER);
                send_encoder->write_ex(false, true, hdr);
                hdr = unistream::get_header(area, unistream::Type::QPACK_DECODER);
                send_decoder->write_ex(false, true, hdr);
                return ctrl.write_settings(write_settings);
            }

           private:
            bool accept_uni_stream(std::shared_ptr<QuicRecvStream> s) {
                if (recv_decoder && recv_encoder && ctrl.recv_control) {
                    s->request_stop_sending(std::uint64_t(http3::H3Error::H3_STREAM_CREATION_ERROR));
                    return false;
                }
                QuicRecvStream* q = s.get();
                Reader* r = s->get_receiver_ctx().get();
                futils::byte buf[8];
                auto data = r->peek(buf);
                unistream::UniStreamHeader head;
                binary::reader reader{data};
                if (!head.parse(reader)) {
                    return false;
                }
                auto ok = r->read(view::wvec(buf, head.type.len));
                if (ok.first.size() != head.type.len) {
                    q->request_stop_sending(std::uint64_t(http3::H3Error::H3_INTERNAL_ERROR));
                    return false;
                }
                switch (unistream::Type(head.type.value)) {
                    case unistream::Type::QPACK_ENCODER:
                        if (recv_encoder) {
                            q->request_stop_sending(std::uint64_t(http3::H3Error::H3_STREAM_CREATION_ERROR));
                            return false;
                        }
                        recv_encoder = s;
                        progress_recv_encoder();
                        return true;
                    case unistream::Type::QPACK_DECODER:
                        if (recv_decoder) {
                            q->request_stop_sending(std::uint64_t(http3::H3Error::H3_STREAM_CREATION_ERROR));
                            return false;
                        }
                        recv_decoder = s;
                        progress_recv_decoder();
                        return true;
                    case unistream::Type::CONTROL:
                        if (ctrl.recv_control) {
                            q->request_stop_sending(std::uint64_t(http3::H3Error::H3_STREAM_CREATION_ERROR));
                            return false;
                        }
                        ctrl.recv_control = s;
                        progress_recv_control();
                        return true;
                    default:
                        q->request_stop_sending(std::uint64_t(http3::H3Error::H3_STREAM_CREATION_ERROR));
                        return false;
                }
            }

            void progress_recv_decoder() {
                if (recv_decoder) {
                    Reader* r = recv_decoder.get()->get_receiver_ctx().get();
                    r->read_best([&](auto& data) {
                        table.dec_recv_stream.stream([&](binary::writer& w) {
                            w.write(data);
                        });
                        return true;
                    });
                    table.read_decoder_stream();
                }
            }

            void progress_recv_encoder() {
                if (recv_encoder) {
                    Reader* r = recv_encoder.get()->get_receiver_ctx().get();
                    r->read_best([&](auto& data) {
                        table.enc_recv_stream.stream([&](binary::writer& w) {
                            w.write(data);
                        });
                        return true;
                    });
                    table.read_encoder_stream();
                }
            }

            void progress_recv_control() {
                if (ctrl.recv_control) {
                    Reader* r = ctrl.recv_control.get()->get_receiver_ctx().get();
                    r->read_best([&](auto& data) {
                        ctrl.read_buf.append(data);
                        ctrl.read_settings([&](std::uint64_t id, std::uint64_t value) {
                            if (id == std::uint64_t(SettingID::max_field_section_size)) {
                                table.enc.set_max_capacity(value);
                            }
                        });
                        return true;
                    });
                }
            }

           public:
            bool read_uni_stream(std::shared_ptr<QuicRecvStream> s) {
                if (!s) {
                    return false;
                }
                auto locked = lock();
                if (s == recv_decoder) {
                    progress_recv_decoder();
                    return true;
                }
                if (s == recv_encoder) {
                    progress_recv_encoder();
                    return true;
                }
                if (s == ctrl.recv_control) {
                    progress_recv_control();
                    return true;
                }
                return accept_uni_stream(s);
            }

            quic::AppError app_err;
            std::uint64_t peer_max_push_id = 0;

           private:
            using Lock = typename Config::conn_lock;
            Lock locker;
            auto lock() {
                locker.lock();
                return helper::defer([&] {
                    locker.unlock();
                });
            }

            void progress_qpack_stream() {
                if (send_decoder && table.dec_send_stream.size()) {
                    QuicSendStream* q = send_decoder.get();
                    table.dec_send_stream.stream([&](binary::writer& w) {
                        q->write_ex(false, true, w.written());
                        w.commit();
                    });
                }
                if (send_encoder && table.enc_send_stream.size()) {
                    QuicSendStream* q = send_encoder.get();
                    table.enc_send_stream.stream([&](binary::writer& w) {
                        q->write_ex(false, true, w.written());
                        w.commit();
                    });
                }
                progress_recv_decoder();
                progress_recv_encoder();
            }

           public:
            bool read_field_section(quic::stream::StreamID id, binary::reader& r, auto&& read) {
                auto locked = lock();
                auto err = table.read_header(id, r, read);
                if (err != qpack::QpackError::none) {
                    if (err == qpack::QpackError::input_length) {
                        return false;
                    }
                    app_err.set_error_code(H3Error::H3_INTERNAL_ERROR);
                    return false;
                }
                progress_qpack_stream();
                return true;
            }

            // write should be void(add_entry,add_field)
            // add_entry is bool(header,value)
            // add_field is bool(header,value,FieldPolicy policy=FieldPolicy::proior_static)
            // after write, the encoder and decoder stream will be written if there is any data
            H3Error write_field_section(quic::stream::StreamID id, binary::writer& w, auto&& write) {
                auto locked = lock();
                auto err = table.write_header(id, w, [&](auto&& add_entry, auto&& add_field) {
                    auto add_entry_wrap = qpack::http3_entry_validate_wrapper(add_entry);
                    auto add_field_wrap = qpack::http3_field_validate_wrapper(add_field);
                    write(add_entry, add_field);
                });
                if (err != qpack::QpackError::none) {
                    if (err == qpack::QpackError::output_length) {
                        return H3Error::H3_INTERNAL_ERROR;
                    }
                    app_err.set_error_code(H3Error::H3_INTERNAL_ERROR);
                    return H3Error::H3_INTERNAL_ERROR;
                }
                return H3Error::H3_NO_ERROR;
            }

            void set_error(H3Error err) {
                auto locked = lock();
                app_err.set_error_code(err);
            }
        };

        enum class StateMachine {
            no_state_machine,
            client_start,  // client header send
            client_header_send = client_start,
            client_data_send,
            client_header_recv,
            client_data_recv,
            client_end,
            server_start,  // server header recv
            server_header_recv = server_start,
            server_data_recv,
            server_header_send,
            server_data_send,
            server_end,

            failed,
        };

        template <class Config>
        struct RequestStream {
            using QuicStream = quic::stream::impl::BidiStream<typename Config::quic_config>;
            using Reader = quic::stream::impl::RecvSorter<typename Config::reader_lock>;
            std::weak_ptr<QuicStream> stream;
            std::shared_ptr<Connection<Config>> conn;
            flex_storage read_buf;
            using String = typename Config::qpack_config::string;
            StateMachine state = StateMachine::no_state_machine;

           private:
            bool is_valid_state_impl(StateMachine s) {
                return state == s;
            }

            bool is_valid_state_impl(StateMachine s1, StateMachine s2, auto&&... s) {
                return state == s1 || is_valid_state_impl(s2, s...);
            }

            bool is_valid_state(StateMachine s, auto&&... ss) {
                return state == StateMachine::no_state_machine || is_valid_state_impl(s, ss...);
            }

            void set_state(StateMachine s) {
                if (state != StateMachine::no_state_machine) {
                    state = s;
                }
            }

            void set_failed() {
                state = StateMachine::failed;
            }

           public:
            // write should be void(add_entry,add_field)
            // add_entry is bool(header,value)
            // add_field is bool(header,value,FieldPolicy policy=FieldPolicy::proior_static)
            H3Error write_header(bool fin, auto&& write) {
                auto s = stream.lock();
                QuicStream* q = s.get();
                if (!q || q->sender.is_fin()) {
                    return H3Error::H3_STREAM_CREATION_ERROR;
                }
                if (!is_valid_state(StateMachine::client_header_send, StateMachine::server_header_send)) {
                    return H3Error::H3_INTERNAL_ERROR;
                }
                binary::WriteStreamingBuffer<String> buf;
                view::wvec section;
                if (auto err = buf.stream([&](auto& w) {
                        Connection<Config>* c = conn.get();
                        if (auto err = c->write_field_section(q->sender.id(), w, write); err != H3Error::H3_NO_ERROR) {
                            return err;
                        }
                        section = w.written();
                        return H3Error::H3_NO_ERROR;
                    });
                    err != H3Error::H3_NO_ERROR) {
                    return err;
                }
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::HEADER, section.size());
                auto res = q->sender.write_ex(fin, true, header, section);
                if (res.result != quic::IOResult::ok) {
                    set_failed();
                    return H3Error::H3_INTERNAL_ERROR;
                }
                if (state == StateMachine::client_start) {
                    set_state(StateMachine::client_data_send);
                }
                else {
                    set_state(StateMachine::server_data_send);
                }
                return H3Error::H3_NO_ERROR;
            }

           private:
            bool append_to_read_buf() {
                auto locked = stream.lock();
                QuicStream* q = locked.get();
                if (!q) {
                    return false;
                }
                return stream::append_to_read_buf(q->receiver, read_buf);
            }

           public:
            bool read_header(auto&& read) {
                auto locked = stream.lock();
                QuicStream* q = locked.get();
                if (!q) {
                    return false;
                }
                if (!is_valid_state(StateMachine::client_header_recv, StateMachine::server_header_recv)) {
                    return false;
                }
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
                    r.reset_buffer(read_buf);
                }
                auto offset = r.offset();
                r.reset_buffer(hdrs.encoded_field);
                auto err = conn->read_field_section(q->receiver.id(), r, read);
                if (!err) {
                    return false;
                }
                read_buf.shift_front(offset);
                if (state == StateMachine::client_start) {
                    set_state(StateMachine::client_data_recv);
                }
                else {
                    set_state(StateMachine::server_data_recv);
                }
                if (q->receiver.is_closed()) {
                    if (state == StateMachine::client_data_recv) {
                        set_state(StateMachine::client_end);
                    }
                    else if (state == StateMachine::server_data_recv) {
                        set_state(StateMachine::server_header_send);
                    }
                }
                return true;
            }

            bool read(auto&& callback) {
                auto locked = stream.lock();
                QuicStream* q = locked.get();
                if (!q) {
                    return false;
                }
                if (q->receiver.is_closed()) {
                    if (state == StateMachine::client_data_recv) {
                        set_state(StateMachine::client_end);
                    }
                    else if (state == StateMachine::server_data_recv) {
                        set_state(StateMachine::server_header_send);
                    }
                }
                if (!is_valid_state(StateMachine::client_data_recv, StateMachine::server_data_recv)) {
                    return false;
                }
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
                    r.reset_buffer(read_buf);
                }
                callback(hdrs.data);
                read_buf.shift_front(r.offset());
                return true;
            }

           private:
            H3Error do_write(QuicStream* q, view::rvec data, bool fin) {
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::DATA, data.size());
                auto res = q->sender.write_ex(fin, true, header, data);
                if (res.result != quic::IOResult::ok) {
                    return H3Error::H3_INTERNAL_ERROR;
                }
                return H3Error::H3_NO_ERROR;
            }

           public:
            H3Error write(view::rvec data, bool fin) {
                auto locked = stream.lock();
                QuicStream* q = locked.get();
                if (!q || q->sender.is_fin()) {
                    return H3Error::H3_STREAM_CREATION_ERROR;
                }
                if (!is_valid_state(StateMachine::client_data_send, StateMachine::server_data_send)) {
                    return H3Error::H3_INTERNAL_ERROR;
                }
                return do_write(q, data, fin);
            }

            H3Error write(auto&& cb) {
                auto locked = stream.lock();
                QuicStream* q = locked.get();
                if (!q || q->sender.is_fin()) {
                    return H3Error::H3_STREAM_CREATION_ERROR;
                }
                if (!is_valid_state(StateMachine::client_data_send, StateMachine::server_data_send)) {
                    return H3Error::H3_INTERNAL_ERROR;
                }
                auto invoke = [&](view::rvec data, bool fin) {
                    if (auto err = do_write(q, data, fin); err != H3Error::H3_NO_ERROR) {
                        return err;
                    }
                    if (fin) {
                        if (state == StateMachine::client_data_send) {
                            set_state(StateMachine::client_end);
                        }
                        else {
                            set_state(StateMachine::server_end);
                        }
                    }
                    return H3Error::H3_NO_ERROR;
                };
                if constexpr (std::is_invocable_r_v<H3Error, decltype(cb), decltype(invoke)>) {
                    return cb(invoke);
                }
                else if constexpr (std::is_invocable_r_v<bool, decltype(cb), decltype(invoke)>) {
                    return cb(invoke) ? H3Error::H3_NO_ERROR : H3Error::H3_INTERNAL_ERROR;
                }
                else {
                    cb(invoke);
                    return H3Error::H3_NO_ERROR;
                }
            }

            // write should be void(add_entry,add_field)
            // add_entry is bool(header,value)
            // add_field is bool(header,value,FieldPolicy policy=FieldPolicy::proior_static)
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
                binary::WriteStreamingBuffer<String> buf;
                view::wvec section;
                if (!buf.stream([&](auto& w) {
                        if (!conn->write_field_section(stream->sender.id(), w, write)) {
                            return false;
                        }
                        section = w.written();
                        return true;
                    })) {
                    return false;
                }
                frame::FrameHeaderArea area;
                auto header = frame::get_header(area, frame::Type::PUSH_PROMISE, quic::varint::len(push_id) + section.size());
                quic::stream::core::StreamWriteBufferState res = q->sender.write_ex(false, true, header, id_w.written(), section);
                if (res.result != quic::IOResult::ok) {
                    return false;
                }
                return true;
            }
        };

        }  // namespace fnet::http3::stream
}  // namespace futils
