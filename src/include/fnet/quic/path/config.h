/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>

namespace utils {
    namespace fnet::quic::path {
        // default for QUIC
        struct Config {
            std::uint64_t max_probes = 3;
            std::uint64_t min_plpmtu = 1200;
            std::uint64_t max_plpmtu = 0xffff;
            std::uint64_t base_plpmtu = 1200;
        };

    }  // namespace fnet::quic::path
}  // namespace utils
