/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

namespace utils {
    namespace fnet::quic::close {
        enum class CloseReason {
            not_closed,
            handshake_timeout,
            idle_timeout,
            close_by_remote,
            close_by_local_runtime,
            close_by_local_app,
        };

        enum class ErrorType {
            none,
            runtime,
            app,
            remote,
        };

        namespace flags {
            enum CloserFlags : byte {
                flag_sent = 0x01,
                flag_received = 0x02,
                flag_should_send_again = 0x04,
            };

            struct CloseFlag {
               private:
                byte flag = 0;

               public:
                constexpr void reset() {
                    flag = 0;
                }

                constexpr void set_sent() {
                    flag |= flag_sent;
                }

                constexpr void set_received() {
                    flag |= flag_received;
                }

                constexpr void set_should_send_again(bool f) {
                    if (f) {
                        flag |= flag_should_send_again;
                    }
                    else {
                        flag &= ~flag_should_send_again;
                    }
                }

                constexpr bool is_sent() const {
                    return flag & flag_sent;
                }

                constexpr bool is_recevied() const {
                    return flag & flag_received;
                }

                constexpr bool should_send_again() const {
                    return flag & flag_should_send_again;
                }
            };
        }  // namespace flags

        struct ClosedContext {
            storage payload;
            time::Timer close_timeout;
            flags::CloseFlag flag;
            time::Clock clock;

            std::pair<view::rvec, bool> create_udp_payload() {
                if (close_timeout.timeout(clock)) {
                    return {{}, false};
                }
                if (flag.is_sent() && flag.should_send_again()) {
                    flag.set_should_send_again(false);
                    return {payload, true};
                }
                return {{}, true};
            }

            bool parse_udp_payload(view::rvec) {
                if (flag.is_sent()) {
                    flag.set_should_send_again(true);
                    return true;
                }
                return false;
            }
        };

        struct Closer {
           private:
            std::atomic<ErrorType> errtype;
            error::Error conn_err;
            storage close_data;
            flags::CloseFlag flag;

           public:
            void reset() {
                conn_err = error::none;
                close_data.clear();
                flag.reset();
                errtype.store(ErrorType::none);
            }

            void on_error(auto&& err, ErrorType reason) {
                errtype.store(reason);
                conn_err = std::forward<decltype(err)>(err);
            }

            view::rvec sent_packet() const {
                if (!flag.is_sent()) {
                    return {};
                }
                return close_data;
            }

            const error::Error& get_error() const {
                return conn_err;
            }

            // thread safe call
            ErrorType error_type() const {
                return errtype;
            }

            // thread safe call
            bool has_error() const {
                return errtype != ErrorType::none;
            }

            bool has_error(const auto& err) const {
                return conn_err == err;
            }

            bool send(frame::fwriter& fw) {
                if (flag.is_recevied()) {
                    return false;
                }
                frame::ConnectionCloseFrame cclose;
                auto qerr = conn_err.as<QUICError>();
                if (qerr) {
                    cclose.reason_phrase = qerr->msg;
                    cclose.error_code = std::uint64_t(qerr->transport_error);
                    cclose.frame_type = std::uint64_t(qerr->frame_type);
                    if (qerr->is_app) {
                        cclose.type = FrameType::CONNECTION_CLOSE_APP;
                    }
                    return fw.write(cclose);
                }
                auto aerr = conn_err.as<AppError>();
                if (aerr) {
                    flex_storage fxst;
                    aerr->msg.error(fxst);
                    cclose.type = FrameType::CONNECTION_CLOSE_APP;
                    cclose.error_code = aerr->error_code;
                    cclose.reason_phrase = fxst;
                    return fw.write(cclose);
                }
                flex_storage fxst;
                conn_err.error(fxst);
                cclose.reason_phrase = fxst;
                cclose.error_code = std::uint64_t(TransportError::INTERNAL_ERROR);
                return fw.write(cclose);
            }

            void on_close_packet_sent(view::rvec close_packet) {
                flag.set_sent();
                close_data = make_storage(close_packet);
            }

            void on_close_retransmited() {
                if (!flag.is_sent()) {
                    return;
                }
                flag.set_should_send_again(false);
            }

            bool should_retransmit() const {
                return flag.is_sent() && flag.should_send_again();
            }

            void on_recv_after_close_sent() {
                flag.set_should_send_again(true);
            }

            // returns (accepted)
            bool recv(const frame::ConnectionCloseFrame& cclose) {
                if (flag.is_recevied() || flag.is_sent()) {
                    return false;  // ignore
                }
                flag.set_received();
                errtype.store(ErrorType::remote);
                close_data = make_storage(cclose.len() + 1);
                close_data.fill(0);
                io::writer w{close_data};
                cclose.render(w);
                frame::ConnectionCloseFrame new_close;
                io::reader r{w.written()};
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
                return flag.is_recevied();
            }

            // expose_closed_context expose context required to close
            // after call this, Closer become invalid
            ClosedContext expose_closed_context(time::Clock clock, time::Time close_deadline) {
                if (!flag.is_sent() && !flag.is_recevied()) {
                    return {};
                }
                time::Timer t;
                t.set_deadline(close_deadline);
                return ClosedContext{
                    .payload = flag.is_sent() ? std::move(close_data) : storage{},
                    .close_timeout = t,
                    .flag = flag,
                    .clock = clock,
                };
            }
        };

    }  // namespace fnet::quic::close
}  // namespace utils
