/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// bitmask - bit mask
#pragma once
#include <type_traits>
#include <limits>

namespace utils {
    namespace number {
        template <class T>
        using Us = std::make_unsigned_t<T>;

        template <class T>
        constexpr T msb_off = T(~Us<T>(0) >> 1);

        template <class T>
        constexpr T msb_on = T(~Us<T>(~Us<T>(0) >> 1));

        template <class T, size_t bit>
        constexpr T bit_on = bit >= sizeof(T) * 8 ? throw "invalid bit number" : T(Us<T>(1) << bit);

        template <class T, size_t bit>
        constexpr T bit_off = T(~bit_on<Us<T>, bit>);

        static_assert((std::numeric_limits<std::uint32_t>::max)() == (msb_off<std::uint32_t> | msb_on<std::uint32_t>),
                      "implementation failed");

    }  // namespace number
}  // namespace utils
