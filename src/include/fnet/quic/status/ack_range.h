/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>

namespace futils {
    namespace fnet::quic::status {
        struct ACKRange {
            std::uint64_t largest;
            std::uint64_t smallest;
        };

        constexpr std::uint64_t decode_ack_delay(std::uint64_t base, std::uint64_t exponent) {
            return base * (1 << exponent);
        }

        struct ECNCounts {
            std::uint64_t ect0 = 0;
            std::uint64_t ect1 = 0;
            std::uint64_t ecn_ce = 0;
        };

    }  // namespace fnet::quic::status
}  // namespace futils
