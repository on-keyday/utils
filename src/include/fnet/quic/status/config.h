/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "constant.h"

namespace utils {
    namespace fnet::quic::status {
        struct Config {
            std::uint64_t window_initial_factor = default_window_initial_factor;
            std::uint64_t window_minimum_factor = default_window_minimum_factor;

            // milliseconds
            time::utime_t handshake_timeout = 0;
            // milliseconds
            time::utime_t handshake_idle_timeout = 0;
            // milliseconds
            time::utime_t initial_rtt = default_initial_rtt;

            std::uint64_t packet_order_threshold = default_packet_threshold;
            Ratio time_threshold = default_time_threshold;
            std::uint64_t delay_ack_packet_count = 2;
            bool use_ack_delay = true;

            Ratio pacer_N = default_pacer_N;

            time::Clock clock;

            std::uint64_t persistent_congestion_threshold = default_persistent_congestion_threshold;

            // milliseconds
            time::utime_t ping_duration = 0;

            // works on server mode
            bool retry_required = false;
        };

        // include duplicated with transport parameter
        struct InternalConfig : Config {
            // milliseconds
            time::utime_t idle_timeout = 0;
            std::uint64_t local_ack_delay_exponent = default_ack_delay_exponent;
            // milliseconds
            std::uint64_t local_max_ack_delay = 0;
        };

        struct PayloadSize {
           private:
            std::uint64_t max_udp_payload_size = 0;

           public:
            constexpr void reset(std::uint64_t max_payload_size) {
                max_udp_payload_size = max_payload_size;
            }

            constexpr bool update_payload_size(std::uint64_t size) noexcept {
                if (size < max_udp_payload_size) {
                    return false;
                }
                max_udp_payload_size = size;
                return true;
            }

            constexpr std::uint64_t current_max_payload_size() const noexcept {
                return max_udp_payload_size;
            }
        };

    }  // namespace fnet::quic::status
}  // namespace utils
