/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// idle_timer - connection idle timer
#pragma once
#include "../time.h"
#include "constant.h"

namespace utils {
    namespace dnet::quic::ack {
        struct IdleTimer {
            time::utime_t idle_timeout = 0;
            time::utime_t handshake_timeout = 0;
            bool handshake_confirmed = false;
            time::Time last_recv_time = time::invalid;
            time::Time first_ack_eliciting_packet_after_idle_sent_time = time::invalid;

            constexpr time::utime_t current_timeout() const {
                return handshake_confirmed ? idle_timeout : handshake_timeout;
            }

            constexpr bool is_idle_timeout(time::Clock& clock) const {
                if (!last_recv_time.valid() && !first_ack_eliciting_packet_after_idle_sent_time.valid()) {
                    return false;
                }
                const auto timeout = current_timeout() * clock.granularity;
                if (timeout == 0) {
                    return false;
                }
                const auto now = clock.now();
                if (!last_recv_time.valid()) {
                    return first_ack_eliciting_packet_after_idle_sent_time + timeout <= now;
                }
                if (!first_ack_eliciting_packet_after_idle_sent_time.valid()) {
                    return last_recv_time + timeout <= now;
                }
                const auto base = min_(time::time_t(last_recv_time), time::time_t(first_ack_eliciting_packet_after_idle_sent_time));
                return base + timeout <= now;
            }

            constexpr void on_packet_recieved(time::Time now) {
                last_recv_time = now;
                first_ack_eliciting_packet_after_idle_sent_time = time::invalid;
            }

            constexpr void on_packet_sent(time::Time now, bool ack_eliciting) {
                if (!first_ack_eliciting_packet_after_idle_sent_time.valid() &&
                    ack_eliciting) {
                    first_ack_eliciting_packet_after_idle_sent_time = now;
                }
            }
        };
    }  // namespace dnet::quic::ack
}  // namespace utils
