/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include "path.h"

namespace utils {
    namespace fnet::quic::path {
        // default for QUIC
        struct MTUConfig {
            std::uint64_t max_probes = 3;
            std::uint64_t min_plpmtu = 1200;
            std::uint64_t max_plpmtu = 0xffff;
            std::uint64_t base_plpmtu = 1200;
        };

        struct PathConfig {
            MTUConfig mtu;
            // because of vulnerability of path validation, we should limit PATH_CHALLENGE buffer size
            // https://seemann.io/posts/2023-12-18-exploiting-quics-path-validation/
            std::uint64_t max_path_challenge = 256;
            path::PathID original_path = path::original_path;
        };

    }  // namespace fnet::quic::path
}  // namespace utils
