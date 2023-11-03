/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include "external/random.h"
#include "external/id_exporter.h"

namespace utils {
    namespace fnet::quic::connid {
        enum class ConnIDChangeMode : unsigned char {
            none,
            constant,
            random,
        };

        struct Config {
            ConnIDChangeMode change_mode = ConnIDChangeMode::random;
            std::uint32_t packet_per_id = 1000;
            std::uint32_t max_packet_per_id = 10000;
            ConnIDExporter exporter{&default_exporter};
            Random random;
            byte connid_len = 4;
            byte concurrent_limit = 4;
        };

    }  // namespace fnet::quic::connid
}  // namespace utils
