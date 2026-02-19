/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// idle_timer - connection idle timer
#pragma once
#include "../time.h"
#include "constant.h"
#include "config.h"
#include "handshake.h"

namespace futils {
    namespace fnet::quic::status {
        struct IdleTimer {
           private:
            time::utime_t idle_timeout = 0;
            time::Time last_recv_time = time::invalid;
            time::Time first_ack_eliciting_packet_after_idle_sent_time = time::invalid;

            constexpr time::utime_t current_timeout(const InternalConfig& config, const HandshakeStatus& hs) const {
                return hs.handshake_confirmed() ? idle_timeout : config.clock.to_clock_granularity(config.handshake_idle_timeout);
            }

           public:
            constexpr void reset() {
                idle_timeout = 0;
                last_recv_time = time::invalid;
                first_ack_eliciting_packet_after_idle_sent_time = time::invalid;
            }

            constexpr time::Time get_deadline(const InternalConfig& config, const HandshakeStatus& hs) const {
                if (!last_recv_time.valid() && !first_ack_eliciting_packet_after_idle_sent_time.valid()) {
                    return time::invalid;
                }
                const auto timeout = current_timeout(config, hs);
                if (timeout == 0) {
                    return time::invalid;
                }
                if (!last_recv_time.valid()) {
                    return first_ack_eliciting_packet_after_idle_sent_time + timeout;
                }
                if (!first_ack_eliciting_packet_after_idle_sent_time.valid()) {
                    return last_recv_time + timeout;
                }
                const auto base = min_(time::time_t(last_recv_time), time::time_t(first_ack_eliciting_packet_after_idle_sent_time));
                return base + timeout;
            }

            constexpr bool timeout(const InternalConfig& config, const HandshakeStatus& hs) const {
                auto deadline = get_deadline(config, hs);
                if (!deadline.valid()) {
                    return false;
                }
                return deadline <= config.clock.now();
            }

            constexpr void on_packet_decrypted(time::Time now) {
                last_recv_time = now;
                first_ack_eliciting_packet_after_idle_sent_time = time::invalid;
            }

            constexpr void on_packet_sent(time::Time now, bool ack_eliciting) {
                if (!first_ack_eliciting_packet_after_idle_sent_time.valid() &&
                    ack_eliciting) {
                    first_ack_eliciting_packet_after_idle_sent_time = now;
                }
            }

            constexpr void apply_idle_timeout(const InternalConfig& config, std::uint64_t peer_idle_timeout) {
                if (peer_idle_timeout == 0) {
                    idle_timeout = config.idle_timeout;
                }
                else if (config.idle_timeout == 0) {
                    idle_timeout = peer_idle_timeout;
                }
                else {
                    idle_timeout = min_(config.idle_timeout, peer_idle_timeout);
                }
                idle_timeout = config.clock.to_clock_granularity(idle_timeout);
            }
        };
    }  // namespace fnet::quic::status
}  // namespace futils
