/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// rtt - round trip time for ACK handling
#pragma once
#include <cstdint>
#include "../time.h"
#include "config.h"

namespace futils {
    namespace fnet::quic::status {

        struct RTT {
           private:
            time::time_t latest_rtt_ = 0;
            time::time_t min_rtt = 0;
            time::time_t smoothed_rtt_ = -1;
            time::time_t rttvar_ = -1;
            time::time_t peer_max_ack_delay_ = time::invalid;
            time::Time first_ack_sample_ = 0;

           public:
            constexpr bool has_first_ack_sample() const {
                return first_ack_sample_ != 0;
            }

            constexpr time::Time first_ack_sample() const {
                return first_ack_sample_;
            }

            constexpr bool reset(const InternalConfig& config) {
                latest_rtt_ = 0;
                min_rtt = 0;
                smoothed_rtt_ = config.clock.to_clock_granularity(config.initial_rtt);
                rttvar_ = smoothed_rtt_ >> 1;  // smoothed_rtt / 2
                peer_max_ack_delay_ = time::invalid;
                first_ack_sample_ = 0;
                return true;
            }

            constexpr void on_connection_migrate(const InternalConfig& config) {
                latest_rtt_ = 0;
                min_rtt = 0;
                smoothed_rtt_ = config.clock.to_clock_granularity(config.initial_rtt);
                rttvar_ = smoothed_rtt_ >> 1;
            }

            // ack_delay MUST be config.clock.granurarity
            constexpr bool sample_rtt(const InternalConfig& config, time::Time time_sent, time::utime_t ack_delay) {
                auto rtt = config.clock.now() - time_sent;
                if (rtt < 0) {
                    return false;
                }
                latest_rtt_ = rtt;
                if (first_ack_sample_ == 0) {
                    min_rtt = rtt;
                    smoothed_rtt_ = rtt;
                    rttvar_ = rtt / 2;
                    first_ack_sample_ = config.clock.now();
                }
                else {
                    min_rtt = min_(min_rtt, latest_rtt_);
                    if (peer_max_ack_delay_ >= 0) {
                        ack_delay = min_(ack_delay, time::utime_t(peer_max_ack_delay_));
                    }
                    auto adjusted_rtt = latest_rtt_;
                    if (latest_rtt_ >= min_rtt + ack_delay) {
                        adjusted_rtt = latest_rtt_ - ack_delay;
                    }
                    auto rttvar_sample = abs(smoothed_rtt_ - adjusted_rtt);
                    // 3/4 * rttvar + 1/4 * rttvar_sample
                    rttvar_ = (3 * rttvar_ + 1 * rttvar_sample) >> 2;
                    // 7/8 * smoothed_rtt + 1/8 * adjusted_rtt
                    smoothed_rtt_ = (7 * smoothed_rtt_ + 1 * adjusted_rtt) >> 3;
                }
                return true;
            }

            constexpr time::time_t smoothed_rtt() const {
                return smoothed_rtt_;
            }

            constexpr time::time_t rttvar() const {
                return rttvar_;
            }

            constexpr time::time_t latest_rtt() const {
                return latest_rtt_;
            }

            constexpr void apply_max_ack_delay(std::uint64_t time) {
                peer_max_ack_delay_ = time;
            }

            constexpr time::time_t max_ack_delay() const {
                return peer_max_ack_delay_ <= 0 ? 0 : peer_max_ack_delay_;
            }
        };

    }  // namespace fnet::quic::status
}  // namespace futils
