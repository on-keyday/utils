/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// limit - datagram limit
#pragma once

namespace utils {
    namespace dnet {
        namespace quic::dgram {
            constexpr size_t initial_datagram_size_limit = 1200;

            constexpr size_t current_limit(size_t dgram_limit, size_t max_udp_datagram_size) {
                if (dgram_limit < max_udp_datagram_size) {
                    return dgram_limit;
                }
                return max_udp_datagram_size;
            }
        }  // namespace quic::dgram
    }      // namespace dnet
}  // namespace utils
