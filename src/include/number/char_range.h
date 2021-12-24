/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// char_range - define char range
#pragma once
#include <cstdint>

namespace utils {
    namespace number {
        // clang-format off
        constexpr std::int8_t number_transform[] = {
        //   0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x00 - 0x0f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x10 - 0x1f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x20 - 0x2f
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, // 0x30 - 0x3f
            -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 0x40 - 0x4f
            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, // 0x50 - 0x5f
            -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 0x60 - 0x6f
            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, // 0x70 - 0x7f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x80 - 0x8f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x90 - 0x9f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xa0 - 0xaf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xb0 - 0xbf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xc0 - 0xcf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xd0 - 0xdf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xe0 - 0xef
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xf0 - 0xff
        };
        // clang-format on

        template <class C>
        constexpr bool is_in_byte_range(C&& c) {
            return c >= 0 && c <= 0xff;
        }

        template <class C>
        constexpr bool is_in_ascii_range(C&& c) {
            return c >= 0 && c <= 0x7f;
        }

        template <class C>
        constexpr bool is_in_visible_range(C&& c) {
            return c >= 0x21 && c <= 0x7e;
        }

        constexpr bool is_radix_char(std::uint8_t c, size_t radix) {
            return number_transform[c] < radix;
        }

        constexpr bool is_digit(std::uint8_t c) {
            return is_radix_char(c, 10);
        }

        constexpr bool is_hex(std::uint8_t c) {
            return is_radix_char(c, 16);
        }

        constexpr bool is_oct(std::uint8_t c) {
            return is_radix_char(c, 8);
        }

        constexpr bool is_bin(std::uint8_t c) {
            return is_radix_char(c, 2);
        }

        constexpr bool is_alnum(std::uint8_t c) {
            return is_radix_char(c, 37);
        }

        constexpr std::uint8_t to_num_char(std::uint8_t n, bool upper = false) {
            if (n <= 9) {
                return '0' + n;
            }
            else if (n >= 10 && n <= 36) {
                if (upper) {
                    return 'A' + (n - 10);
                }
                else {
                    return 'a' + (n - 10);
                }
            }
            else {
                return 0xff;
            }
        }

    }  // namespace number
}  // namespace utils
