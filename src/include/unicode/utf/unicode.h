/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// unicode
#pragma once
#include <cstdint>

namespace utils::unicode {
    constexpr std::uint32_t invalid_code = ~std::uint32_t(0);
    constexpr std::uint32_t max_code = 0x10FFFF;
    constexpr std::uint32_t min_code = 0;
    constexpr std::uint32_t byte_order_mark = 0xFEFF;
    constexpr std::uint32_t replacement_character = 0xFFFD;
    constexpr std::uint32_t surrogate_1 = 0xD800;
    constexpr std::uint32_t surrogate_2 = 0xDC00;
    constexpr std::uint32_t surrogate_3 = 0xE000;

    constexpr bool is_valid_range(auto c) noexcept {
        return (min_code <= c && c < surrogate_1) || (surrogate_3 < c && c <= max_code);
    }
}  // namespace utils::unicode
