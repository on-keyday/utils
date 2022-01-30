/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// radix - radix helper
#pragma once
#include <cstdint>
#include <limits>

namespace utils {
    namespace number {
        template <class T>
        constexpr T radix_base_max(std::uint32_t radix) {
            auto mx = (std::numeric_limits<T>::max)();
            size_t count = 0;
            T t = 1;
            mx /= radix;
            while (mx) {
                mx /= radix;
                t *= radix;
            }
            return t;
        }

        template <class T>
        constexpr T radix_max_cache[37] = {
            0,
            0,
            radix_base_max<T>(2),
            radix_base_max<T>(3),
            radix_base_max<T>(4),
            radix_base_max<T>(5),
            radix_base_max<T>(6),
            radix_base_max<T>(7),
            radix_base_max<T>(8),
            radix_base_max<T>(9),
            radix_base_max<T>(10),
            radix_base_max<T>(11),
            radix_base_max<T>(12),
            radix_base_max<T>(13),
            radix_base_max<T>(14),
            radix_base_max<T>(15),
            radix_base_max<T>(16),
            radix_base_max<T>(17),
            radix_base_max<T>(18),
            radix_base_max<T>(19),
            radix_base_max<T>(20),
            radix_base_max<T>(21),
            radix_base_max<T>(22),
            radix_base_max<T>(23),
            radix_base_max<T>(24),
            radix_base_max<T>(25),
            radix_base_max<T>(26),
            radix_base_max<T>(27),
            radix_base_max<T>(28),
            radix_base_max<T>(29),
            radix_base_max<T>(30),
            radix_base_max<T>(31),
            radix_base_max<T>(32),
            radix_base_max<T>(33),
            radix_base_max<T>(34),
            radix_base_max<T>(35),
            radix_base_max<T>(36),
        };
    }  // namespace number
}  // namespace utils
