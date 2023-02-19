/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// version - QUIC version
#pragma once
#include <cstdint>

namespace utils {
    namespace fnet {
        namespace quic {
            enum Version : std::uint32_t {
                version_negotiation = 0x00000000,
                version_1 = 0x00000001,
            };
        }
    }  // namespace fnet
}  // namespace utils