/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// rtt - round trip time for ACK handling
#pragma once
#include <cstdint>
#include "../time.h"
#include "constant.h"

namespace utils {
    namespace dnet {
        namespace quic::ack {

            struct RTTSampler {
                time::Time time_sent = 0;
            };

            struct RTT {
                time::Time latest_rtt = time::invalid;
                time::Time min_rtt = time::invalid;
                time::Time smoothed_rtt = time::invalid;
                time::Time rttvar = time::invalid;
                time::Time max_ack_delay = time::invalid;
                time::Time first_ack_sample = 0;

                constexpr bool reset() {
                    latest_rtt = time::invalid;
                    min_rtt = time::invalid;
                    smoothed_rtt = time::invalid;
                    rttvar = time::invalid;
                    max_ack_delay = time::invalid;
                    first_ack_sample = 0;
                    return true;
                }

                constexpr void set_inirtt(time::time_t inirtt) {
                    smoothed_rtt = inirtt;
                    rttvar = inirtt >> 1;
                }

                constexpr bool sample_rtt(time::Clock& clock, time::Time time_sent, time::utime_t ack_delay) {
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
                            ack_delay = min_(ack_delay, time::utime_t(max_ack_delay.value));
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
