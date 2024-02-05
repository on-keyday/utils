/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// constant - constant value
#pragma once
#include "../time.h"

namespace futils {
    namespace fnet::quic::status {
        constexpr auto default_initial_rtt = 333;  // 333ms
        constexpr auto default_packet_threshold = 3;

        constexpr auto amplification_factor = 3;

        struct Ratio {
            time::time_t num = 0;
            time::time_t den = 0;
        };

        constexpr auto default_time_threshold = Ratio{9, 8};
        constexpr auto default_pacer_N = Ratio{5, 4};

        constexpr auto abs(auto a) {
            return a < 0 ? -a : a;
        }

        constexpr auto max_(auto a, auto b) {
            return a < b ? b : a;
        }

        constexpr auto min_(auto a, auto b) {
            return a < b ? a : b;
        }

        constexpr auto default_window_initial_factor = 10;
        constexpr auto default_window_minimum_factor = 2;

        constexpr auto default_ack_delay_exponent = 3;

        constexpr auto default_persistent_congestion_threshold = 3;

    }  // namespace fnet::quic::status
}  // namespace futils
