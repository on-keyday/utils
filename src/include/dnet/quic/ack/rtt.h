/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// rtt - round trip time for ACK handling
#pragma once
#include <cstdint>

namespace utils {
    namespace dnet {
        namespace quic::ack {
            using time_t = std::int64_t;
            using utime_t = std::uint64_t;
            struct Clock {
                utime_t granularity = 0;
                void* ctx;
                time_t (*now_fn)(void*);
                constexpr time_t now() const {
                    return now_fn ? now_fn(ctx) : -1;
                }
            };

            struct RTTSampler {
                time_t time_sent = 0;
            };

            constexpr auto abs(auto a) {
                return a < 0 ? -a : a;
            }

            constexpr auto max_(auto a, auto b) {
                return a < b ? b : a;
            }

            constexpr auto min_(auto a, auto b) {
                return a < b ? a : b;
            }

            struct RTT {
                time_t latest_rtt = -1;
                time_t min_rtt = -1;
                time_t smoothed_rtt = -1;
                time_t rttvar = -1;
                time_t max_ack_delay = -1;
                time_t first_ack_sample = 0;

                constexpr bool reset() {
                    latest_rtt = -1;
                    min_rtt = -1;
                    smoothed_rtt = -1;
                    rttvar = -1;
                    max_ack_delay = -1;
                    first_ack_sample = 0;
                    return true;
                }

                constexpr void set_inirtt(time_t inirtt) {
                    smoothed_rtt = inirtt;
                    rttvar = inirtt >> 1;
                }

                constexpr bool sample_rtt(Clock& clock, time_t time_sent, utime_t ack_delay) {
                    auto rtt = clock.now() - time_sent;
                    if (rtt < 0) {
                        return false;
                    }
                    latest_rtt = rtt;
                    if (first_ack_sample == 0) {
                        min_rtt = rtt;
                        smoothed_rtt = rtt;
                        rttvar = rtt / 2;
                        first_ack_sample = clock.now();
                    }
                    else {
                        if (max_ack_delay >= 0) {
                            ack_delay = min_(ack_delay, max_ack_delay);
                        }
                        auto adjusted_rtt = latest_rtt;
                        if (latest_rtt >= min_rtt + ack_delay) {
                            adjusted_rtt = latest_rtt - ack_delay;
                        }
                        // 7/8 * smoothed_rtt + 1/8 * adjusted_rtt
                        smoothed_rtt = (7 * smoothed_rtt + 1 * adjusted_rtt) >> 3;
                        auto rttvar_sample = abs(smoothed_rtt - adjusted_rtt);
                        // 3/4 * rttvar + 1/4 * rttvar_sample
                        rttvar = (3 * rttvar + 1 * rttvar_sample) >> 2;
                    }
                    return true;
                }
            };

        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils
