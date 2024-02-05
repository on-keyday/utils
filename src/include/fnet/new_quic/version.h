/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// version - QUIC version
#pragma once
#include <cstdint>

namespace futils::fnet::quic {

    enum class Version : std::uint32_t {
        version_negotiation = 0x00000000,
        version_1 = 0x00000001,
        version_2 = 0x6b3343cf,
    };

}  // namespace futils::fnet::quic
