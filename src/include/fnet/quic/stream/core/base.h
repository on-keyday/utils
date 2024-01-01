/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
// #include "../../packet_number.h"
#include "../../varint.h"
#include "limiter.h"

namespace futils {
    namespace fnet::quic::stream::core {

        enum class SendState : byte {
            ready,
            send,
            data_sent,
            data_recved,
            reset_sent,
            reset_recved,
        };

        struct SendUniStreamState {
           private:
            Limiter limit;
            std::uint64_t error_code = 0;
            SendState state = SendState::ready;
            enum : byte {
                flag_none = 0x0,
                flag_reset_by_peer = 0x1,
                flag_reset_by_local = 0x2,
                flag_reset_sent = 0x4,
            };
            byte reset_flags = flag_none;

           public:
            constexpr Limiter get_send_limiter() const noexcept {
                return limit;
            }

            constexpr std::uint64_t sendable_size() const noexcept {
                return limit.avail_size();
            }

            constexpr std::uint64_t send_limit() const noexcept {
                return limit.current_limit();
            }

            constexpr std::uint64_t sent_bytes() const noexcept {
                return limit.current_usage();
            }

            constexpr bool update_send_limit(std::uint64_t credit) noexcept {
                return limit.update_limit(credit);
            }

            // set_send_limit is called first time call or transport applying
            constexpr bool set_send_limit(std::uint64_t new_limit) noexcept {
                return limit.set_limit(new_limit);
            }

            // is_terminal_state reports whether stream is terminal state (not to retransmit)
            // state==SendState::data_recved || state==SendState::reset_recved
            constexpr bool is_terminal_state() const noexcept {
                return state == SendState::data_recved ||
                       state == SendState::reset_recved;
            }

            constexpr bool is_ready() const noexcept {
                return state == SendState::ready;
            }

            constexpr bool can_send() const noexcept {
                return state == SendState::ready ||
                       state == SendState::send;
            }

            constexpr bool can_retransmit() const noexcept {
                return state == SendState::send ||
                       state == SendState::data_sent;
            }

            constexpr bool can_reset() const noexcept {
                return state == SendState::ready ||
                       state == SendState::send ||
                       state == SendState::data_sent;
            }

            constexpr bool can_retransmit_reset() const noexcept {
                return state == SendState::reset_sent;
            }

            constexpr bool is_data_sent() const noexcept {
                return state == SendState::data_sent;
            }

            constexpr bool reset() noexcept {
                if (state == SendState::reset_sent ||
                    state == SendState::data_recved ||
                    state == SendState::reset_recved) {
                    return false;
                }
                state = SendState::reset_sent;
                reset_flags |= flag_reset_sent;
                return true;
            }

            constexpr bool reset_ack() noexcept {
                if (state == SendState::reset_recved) {
                    return true;  // already recved
                }
                if (state != SendState::reset_sent) {
                    return false;  // send reset first!
                }
                state = SendState::reset_recved;
                return true;
            }

            // send_max is absolute value, StreamFrame.offset+StreamFrame.length
            constexpr bool send(size_t send_max) noexcept {
                if (!can_send() || !limit.update_use(send_max)) {
                    return false;  // cannot send
                }
                state = SendState::send;
                return true;  // let's send it!
            }

            constexpr bool sent() noexcept {
                if (state != SendState::send) {
                    return false;
                }
                state = SendState::data_sent;
                return true;
            }

            constexpr bool sent_ack() noexcept {
                if (state != SendState::data_sent) {
                    return false;
                }
                state = SendState::data_recved;
                return true;
            }

           private:
            constexpr bool set_error_code_impl(std::uint64_t code, bool by_peer = false) noexcept {
                if (state == SendState::reset_sent ||
                    state == SendState::data_recved ||
                    state == SendState::reset_recved) {
                    return false;
                }
                error_code = code;
                reset_flags |= by_peer ? flag_reset_by_peer : flag_reset_by_local;
                return true;
            }

           public:
            constexpr bool set_error_code(std::uint64_t code) noexcept {
                return set_error_code_impl(code, false);
            }

            constexpr void recv_stop_sending(std::uint64_t code) {
                set_error_code_impl(code, true);
            }

            constexpr bool is_reset_required() const noexcept {
                return !(reset_flags & flag_reset_sent) &&
                       ((reset_flags & flag_reset_by_local) ||
                        (reset_flags & flag_reset_by_peer));
            }

            constexpr std::uint64_t get_error_code() const noexcept {
                return error_code;
            }
        };

        enum class RecvState : byte {
            // pre recv state (no data nor no reset received)
            // this is almost same as recv semantics
            // but in some case, different operation
            pre_recv,
            recv,
            size_known,
            data_recved,
            reset_recved,
            data_read,
            reset_read,
        };

        struct RecvUniStreamState {
           private:
            Limiter limit;
            std::uint64_t error_code = 0;
            std::uint64_t ini_limit = 0;  // cache for update callback
            RecvState state = RecvState::pre_recv;
            bool stop_required = false;
            bool should_send_limit_update = false;

           public:
            constexpr Limiter get_recv_limiter() const noexcept {
                return limit;
            }

            constexpr bool update_recv_limit(std::uint64_t new_limit) noexcept {
                should_send_limit_update = limit.update_limit(new_limit) || should_send_limit_update;
                return should_send_limit_update;
            }

            // set_recv_limit is called first time call or transport parameter applying
            constexpr bool set_recv_limit(std::uint64_t new_limit) noexcept {
                if (limit.set_limit(new_limit)) {
                    ini_limit = new_limit;
                    return true;
                }
                return false;
            }

            constexpr std::uint64_t initial_limit() const noexcept {
                return ini_limit;
            }

            constexpr std::uint64_t recv_limit() const noexcept {
                return limit.current_limit();
            }

            constexpr std::uint64_t recv_bytes() const noexcept {
                return limit.current_usage();
            }

            constexpr bool require_send_limit_update() const noexcept {
                return should_send_limit_update;
            }

            constexpr bool is_stop_required() const noexcept {
                return stop_required;
            }

            // state is RecvState::pre_recv or RecvState::recv
            constexpr bool is_recv() const noexcept {
                return state == RecvState::pre_recv ||
                       state == RecvState::recv;
            }

            constexpr bool can_recv() const noexcept {
                return is_recv() ||
                       state == RecvState::size_known;
            }

            // recv_max is absolute value, StreamFrame.offset+StreamFrame.length
            constexpr bool recv(size_t recv_max) noexcept {
                if (is_size_known() && recv_max > limit.current_usage()) {
                    return false;
                }
                if (!can_recv() || !limit.update_use(recv_max)) {
                    return false;
                }
                if (state != RecvState::size_known) {
                    state = RecvState::recv;
                }
                return true;
            }

            constexpr bool size_known() noexcept {
                if (state == RecvState::size_known) {
                    return true;
                }
                if (state != RecvState::recv) {
                    // not allow RecvState::pre_recv
                    // call recv() before this
                    return false;
                }
                state = RecvState::size_known;
                return true;
            }

            constexpr bool is_pre_recv() const noexcept {
                return state == RecvState::pre_recv;
            }

            // state is RecvState::reset_recved || RecvState::size_known
            constexpr bool is_size_known() const noexcept {
                return state == RecvState::reset_recved ||
                       state == RecvState::size_known;
            }

            constexpr bool can_stop_sending() const noexcept {
                return state != RecvState::reset_recved &&
                       state != RecvState::reset_read &&
                       state != RecvState::data_read;
            }

            constexpr bool reset(size_t fin_size, size_t code) noexcept {
                if (state == RecvState::data_read || state == RecvState::reset_read) {
                    return true;  // needless to reset
                }
                if (is_size_known() && fin_size != limit.current_usage()) {
                    return false;
                }
                if (!limit.update_use(fin_size)) {
                    return false;  // flow control violation
                }
                state = RecvState::reset_recved;
                error_code = code;
                return true;
            }

            constexpr bool stop_sending(std::uint64_t code) noexcept {
                if (!can_stop_sending()) {
                    return false;
                }
                error_code = code;
                stop_required = true;
                return true;
            }

            constexpr void max_stream_data_sent() noexcept {
                should_send_limit_update = false;
            }

            constexpr bool reset_read() noexcept {
                if (state != RecvState::reset_recved) {
                    return false;
                }
                state = RecvState::reset_read;
                return true;
            }

            // all data recved
            constexpr bool recved() noexcept {
                if (state != RecvState::size_known) {
                    return false;
                }
                state = RecvState::data_recved;
                return true;
            }

            // all data read
            constexpr bool data_read() noexcept {
                if (state != RecvState::data_recved) {
                    return false;
                }
                state = RecvState::data_read;
                return true;
            }

            // report whether state is terminal state
            constexpr bool is_terminal_state() const noexcept {
                return state == RecvState::data_read ||
                       state == RecvState::reset_read;
            }

            // report whether all data or reset has been recieved
            constexpr bool is_recv_all_state() const noexcept {
                return state == RecvState::data_recved ||
                       state == RecvState::reset_recved ||
                       is_terminal_state();
            }

            constexpr bool is_reset() const noexcept {
                return state == RecvState::reset_recved ||
                       state == RecvState::reset_read;
            }

            constexpr std::uint64_t get_error_code() const noexcept {
                return error_code;
            }
        };

        namespace test {
            constexpr auto sizeof_recv_stream = sizeof(RecvUniStreamState);
            constexpr auto sizeof_send_stream = sizeof(SendUniStreamState);
        }  // namespace test

        struct EmptyLock {
            constexpr void lock() noexcept {}
            constexpr void unlock() noexcept {};
            constexpr bool try_lock() noexcept {
                return true;
            }
        };

    }  // namespace fnet::quic::stream::core
}  // namespace futils
