/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../storage.h"
#include "../transport_error.h"
#include "../frame/writer.h"
#include "../frame/conn_manage.h"
#include "../time.h"
#include <atomic>
#include "../path/path.h"
#include <binary/flags.h>

namespace futils {
    namespace fnet::quic::close {
        enum class CloseReason {
            not_closed,
            handshake_timeout,
            idle_timeout,
            close_by_remote,
            close_by_local_runtime,
            close_by_local_app,
        };

        enum class ErrorType : byte {
            none,
            runtime,
            app,
            remote,
        };

        namespace flags {

            struct CloseFlag {
               private:
                enum CloserFlags : byte {
                    flag_sent = 0x04,
                    flag_received = 0x02,
                    flag_should_send_again = 0x01,
                };

                binary::flags_t<CloserFlags, 5, 1, 1, 1> flag;

               public:
                constexpr void reset() {
                    flag = 0;
                }

                bits_flag_alias_method(flag, 1, sent);
                bits_flag_alias_method(flag, 2, received);
                bits_flag_alias_method(flag, 3, should_send_again);
            };
        }  // namespace flags

        struct ClosedContext {
            storage payload;
            time::Timer close_timeout;
            flags::CloseFlag flag;
            path::PathID active_path;
            time::Clock clock;
            // exporter.mux pointer
            // this may refer multiplexer object
            std::weak_ptr<void> exporter_mux;

            std::pair<view::rvec, bool> create_udp_payload() {
                if (close_timeout.timeout(clock)) {
                    return {{}, false};
                }
                if (flag.sent() && flag.should_send_again()) {
                    flag.should_send_again(false);
                    return {payload, true};
                }
                return {{}, true};
            }

            bool parse_udp_payload(view::rvec) {
                if (flag.sent()) {
                    flag.should_send_again(true);
                    return true;
                }
                return false;
            }
        };

        struct Closer {
           private:
            std::atomic<ErrorType> err_type;
            flags::CloseFlag flag;
            error::Error conn_err;
            storage close_data;

           public:
            void reset() {
                conn_err = error::none;
                close_data.clear();
                flag.reset();
                err_type.store(ErrorType::none);
            }

            void on_error(auto&& err, ErrorType reason) {
                err_type.store(reason);
                conn_err = std::forward<decltype(err)>(err);
            }

            view::rvec sent_packet() const {
                if (!flag.sent()) {
                    return {};
                }
                return close_data;
            }

            const error::Error& get_error() const {
                return conn_err;
            }

            // thread safe call
            ErrorType error_type() const {
                return err_type;
            }

            // thread safe call
            bool has_error() const {
                return err_type != ErrorType::none;
            }

            bool send(frame::fwriter& fw, PacketType type) {
                if (flag.received()) {
                    return false;
                }
                frame::ConnectionCloseFrame cclose;
                auto qerr = conn_err.as<QUICError>();
                if (qerr) {
                    cclose.reason_phrase = qerr->msg;
                    cclose.error_code = std::uint64_t(qerr->transport_error);
                    cclose.frame_type = std::uint64_t(qerr->frame_type);
                    if (type == PacketType::OneRTT && qerr->is_app) {
                        cclose.type = FrameType::CONNECTION_CLOSE_APP;
                    }
                    else if (qerr->is_app) {
                        // hide phrase for security
                        cclose.reason_phrase = {};
                        cclose.error_code = std::uint64_t(TransportError::APPLICATION_ERROR);
                    }
                    return fw.write(cclose);
                }
                auto aerr = conn_err.as<AppError>();
                if (aerr) {
                    flex_storage fxst;
                    cclose.error_code = aerr->error_code;
                    if (type == PacketType::OneRTT) {
                        cclose.type = FrameType::CONNECTION_CLOSE_APP;
                        aerr->msg.error(fxst);
                        cclose.reason_phrase = fxst;
                    }
                    else {
                        cclose.error_code = std::uint64_t(TransportError::APPLICATION_ERROR);
                    }
                    return fw.write(cclose);
                }
                flex_storage fxst;
                conn_err.error(fxst);
                cclose.reason_phrase = fxst;
                cclose.error_code = std::uint64_t(TransportError::INTERNAL_ERROR);
                return fw.write(cclose);
            }

            void on_close_packet_sent(view::rvec close_packet) {
                flag.sent(true);
                close_data = make_storage(close_packet);
            }

            void on_close_retransmitted() {
                if (!flag.sent()) {
                    return;
                }
                flag.should_send_again(false);
            }

            bool should_retransmit() const {
                return flag.sent() && flag.should_send_again();
            }

            void on_recv_after_close_sent() {
                flag.should_send_again(true);
            }

            // returns (accepted)
            bool recv(const frame::ConnectionCloseFrame& cclose) {
                if (flag.received() || flag.sent()) {
                    return false;  // ignore
                }
                flag.received(true);
                err_type.store(ErrorType::remote);
                close_data = make_storage(cclose.len() + 1);
                close_data.fill(0);
                binary::writer w{close_data};
                cclose.render(w);
                frame::ConnectionCloseFrame new_close;
                binary::reader r{w.written()};
                new_close.parse(r);
                conn_err = QUICError{
                    .msg = new_close.reason_phrase.as_char(),
                    .transport_error = TransportError(new_close.error_code.value),
                    .frame_type = FrameType(new_close.frame_type.value),
                    .is_app = new_close.type.type_detail() == FrameType::CONNECTION_CLOSE_APP,
                    .by_peer = true,
                };
                return true;
            }

            bool close_by_peer() const {
                return flag.received();
            }

            // expose_closed_context expose context required to close
            // after call this, Closer become invalid
            ClosedContext expose_closed_context(time::Clock clock, time::Time close_deadline) {
                if (!flag.sent() && !flag.received()) {
                    return {};
                }
                time::Timer t;
                t.set_deadline(close_deadline);
                return ClosedContext{
                    .payload = flag.sent() ? std::move(close_data) : storage{},
                    .close_timeout = t,
                    .flag = flag,
                    .clock = clock,
                };
            }
        };

    }  // namespace fnet::quic::close
}  // namespace futils
