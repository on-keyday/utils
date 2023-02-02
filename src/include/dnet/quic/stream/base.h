/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "../packet_number.h"
#include "../varint.h"

namespace utils {
    namespace dnet::quic::stream {
        enum class Direction {
            client,
            server,
            unknown,
        };

        enum class StreamType {
            uni,
            bidi,
            unknown,
        };

        constexpr size_t dir_to_mask(Direction dir) {
            switch (dir) {
                case Direction::client:
                    return 0x0;
                case Direction::server:
                    return 0x1;
                default:
                    return ~0;
            }
        }

        constexpr Direction inverse(Direction dir) {
            switch (dir) {
                case Direction::client:
                    return Direction::server;
                case Direction::server:
                    return Direction::client;
                default:
                    return Direction::unknown;
            }
        }

        constexpr size_t type_to_mask(StreamType typ) {
            switch (typ) {
                case StreamType::uni:
                    return 0x2;
                case StreamType::bidi:
                    return 0x0;
                default:
                    return ~0;
            }
        }

        struct StreamID {
            std::uint64_t id = ~0;

            constexpr StreamID() = default;

            constexpr StreamID(std::uint64_t id)
                : id(id) {}

            constexpr bool valid() const {
                return id < (std::uint64_t(0x3) << 62);
            }

            constexpr operator std::uint64_t() const {
                return id;
            }

            constexpr std::uint64_t seq_count() const {
                if (!valid()) {
                    return ~0;
                }
                return id >> 2;
            }

            constexpr StreamType type() const {
                if (!valid()) {
                    return StreamType::unknown;
                }
                if (id & 0x2) {
                    return StreamType::uni;
                }
                else {
                    return StreamType::bidi;
                }
            }

            constexpr Direction dir() const {
                if (!valid()) {
                    return Direction::unknown;
                }
                if (id & 0x1) {
                    return Direction::server;
                }
                else {
                    return Direction::client;
                }
            }
        };

        constexpr StreamID make_id(std::uint64_t seq, Direction dir, StreamType type) noexcept {
            return (seq << 2) | type_to_mask(type) | dir_to_mask(dir);
        }

        constexpr StreamID invalid_id = ~0;

        struct Limiter {
           private:
            std::uint64_t limit = 0;
            std::uint64_t used = 0;

           public:
            constexpr Limiter() = default;
            constexpr std::uint64_t avail_size() const {
                return limit - used;
            }

            constexpr bool on_limit(std::uint64_t s) const {
                return avail_size() < s;
            }

            constexpr bool update_limit(std::uint64_t new_limit) {
                if (new_limit >= varint::border(8)) {
                    return false;
                }
                if (new_limit > limit) {
                    limit = new_limit;
                    return true;
                }
                return false;
            }

            constexpr bool use(std::uint64_t s) {
                if (on_limit(s)) {
                    return false;
                }
                used += s;
                return true;
            }

            constexpr bool update_use(std::uint64_t new_used) {
                if (limit < new_used) {
                    return false;
                }
                if (new_used > used) {
                    used = new_used;
                }
                return true;
            }

            constexpr std::uint64_t curlimit() const {
                return limit;
            }

            constexpr std::uint64_t curused() const {
                return used;
            }

            bool unuse(std::uint64_t l) {
                if (used < l) {
                    return false;
                }
                used -= l;
                return true;
            }
        };

        enum class SendState {
            ready,
            send,
            data_sent,
            data_recved,
            reset_sent,
            reset_recved,
        };

        struct SendUniStreamState {
            Limiter limit;
            std::uint64_t error_code = 0;
            SendState state = SendState::ready;
            bool require_reset = false;

            constexpr bool is_terminal_state() const noexcept {
                return state == SendState::data_recved ||
                       state == SendState::reset_recved;
            }

            constexpr bool can_send() const noexcept {
                return state == SendState::ready ||
                       state == SendState::send;
            }

            constexpr bool can_retransmit() const noexcept {
                return state == SendState::send ||
                       state == SendState::data_sent;
            }

            constexpr bool reset() noexcept {
                if (state == SendState::reset_sent ||
                    state == SendState::data_recved ||
                    state == SendState::reset_recved) {
                    return false;
                }
                state = SendState::reset_sent;
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

            constexpr bool set_error_code(std::uint64_t code) {
                if (state == SendState::reset_sent ||
                    state == SendState::data_recved ||
                    state == SendState::reset_recved) {
                    return false;
                }
                error_code = code;
                require_reset = true;
                return true;
            }

            constexpr void recv_stop_sending(std::uint64_t code) {
                set_error_code(code);
            }
        };

        enum class RecvState {
            // pre recv state (no data or no reset recved)
            // this is same as recv semantics
            pre_recv,
            recv,
            size_known,
            data_recved,
            reset_recved,
            data_read,
            reset_read,
        };

        struct RecvUniStreamState {
            Limiter limit;
            std::uint64_t error_code = 0;
            RecvState state = RecvState::pre_recv;
            bool stop_required = false;
            bool should_send_limit_update = false;

            bool update_recv_limit(std::uint64_t new_limit) {
                should_send_limit_update = limit.update_limit(new_limit) || should_send_limit_update;
                return should_send_limit_update;
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
                if (is_size_known() && recv_max > limit.curused()) {
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
                if (is_size_known() && fin_size != limit.curused()) {
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
        };

        struct EmptyLock {
            constexpr void lock() noexcept {}
            constexpr void unlock() noexcept {};
            constexpr bool try_lock() noexcept {
                return true;
            }
        };

        constexpr auto do_lock(auto& locker) {
            locker.lock();
            return helper::defer([&] {
                locker.unlock();
            });
        }

        namespace internal {
            constexpr bool do_try_locks() {
                return true;
            }

            constexpr bool do_try_locks(auto& cur, auto&... rem) {
                auto d = helper::defer([&] {
                    cur.unlock();
                });
                if (!cur.try_lock()) {
                    return false;
                }
                if (!do_try_locks(rem...)) {
                    return false;
                }
                d.cancel();
                return true;
            }

            constexpr bool do_multi_lock_impl(auto& a, auto&&... rem) {
                a.lock();
                auto d = helper::defer([&] {
                    a.unlock();
                });
                if (!do_try_locks(rem...)) {
                    return false;
                }
                d.cancel();
                return true;
            }
        }  // namespace internal

        constexpr auto do_multi_lock(auto&... locker) {
            while (!internal::do_multi_lock_impl(locker...)) {
                // busy loop
            }
            return helper::defer([&] {
                auto unlock = [&](auto& l) { l.unlock();return true; };
                (... && unlock(locker));
            });
        }

    }  // namespace dnet::quic::stream
}  // namespace utils

namespace std {
    template <class T>
    struct hash;

    template <>
    struct hash<utils::dnet::quic::stream::StreamID> {
        constexpr auto operator()(auto&& id) const noexcept {
            return std::hash<std::uint64_t>{}(id.id);
        }
    };
}  // namespace std
