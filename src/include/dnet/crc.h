/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crc - CRC-32
#pragma once
#include "../view/iovec.h"

namespace utils {
    namespace dnet::crc {
        struct CRC32Table {
            std::uint32_t table[256];
        };

        constexpr CRC32Table make_crc_table() noexcept {
            CRC32Table crc;
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int j = 0; j < 8; j++) {
                    c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
                }
                crc.table[i] = c;
            }
            return crc;
        }

        constexpr CRC32Table crc_table = make_crc_table();

        constexpr std::uint32_t crc32(view::rvec input) noexcept {
            std::uint32_t c = 0xffffffff;
            for (auto i : input) {
                c = crc_table.table[(c ^ i) & 0xFF] ^ (c >> 8);
            }
            return c ^ 0xFFFFFFFF;
        };

    }  // namespace dnet::crc
}  // namespace utils
