/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// radix - radix helper
#pragma once
#include <cstdint>
#include <limits>
#include "char_range.h"
#include "array.h"

namespace utils {
    namespace number {
        template <class T>
        constexpr T radix_base_max(std::uint32_t radix) {
            auto mx = (std::numeric_limits<T>::max)();
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

        template <class T>
        constexpr size_t get_digit_count(int radix) {
            if (!acceptable_radix(radix)) {
                return {};
            }
            auto mx = radix_max_cache<T>[radix];
            size_t i = 0;
            while (mx != 1) {
                mx /= radix;
                i++;
            }
            return i;
        }

        template <class T, size_t radix>
        constexpr size_t digit_count = get_digit_count<T>(radix);

        template <size_t mx, size_t radix, class T, size_t size = digit_count<T, radix>>
        constexpr Array<T, mx> get_digit_bound() {
            Array<T, mx> res{};
            auto base = radix;
            for (size_t i = 0; i < size; i++) {
                res.push_back(base);
                base *= radix;
            }
            return res;
        }

        template <class T, size_t mx = digit_count<T, 2>>
        constexpr Array<T, mx> digit_bound[37]{
            {},
            {},
            get_digit_bound<mx, 2, T>(),
            get_digit_bound<mx, 3, T>(),
            get_digit_bound<mx, 4, T>(),
            get_digit_bound<mx, 5, T>(),
            get_digit_bound<mx, 6, T>(),
            get_digit_bound<mx, 7, T>(),
            get_digit_bound<mx, 8, T>(),
            get_digit_bound<mx, 9, T>(),
            get_digit_bound<mx, 10, T>(),
            get_digit_bound<mx, 11, T>(),
            get_digit_bound<mx, 12, T>(),
            get_digit_bound<mx, 13, T>(),
            get_digit_bound<mx, 14, T>(),
            get_digit_bound<mx, 15, T>(),
            get_digit_bound<mx, 16, T>(),
            get_digit_bound<mx, 17, T>(),
            get_digit_bound<mx, 18, T>(),
            get_digit_bound<mx, 19, T>(),
            get_digit_bound<mx, 20, T>(),
            get_digit_bound<mx, 21, T>(),
            get_digit_bound<mx, 22, T>(),
            get_digit_bound<mx, 23, T>(),
            get_digit_bound<mx, 24, T>(),
            get_digit_bound<mx, 25, T>(),
            get_digit_bound<mx, 26, T>(),
            get_digit_bound<mx, 27, T>(),
            get_digit_bound<mx, 28, T>(),
            get_digit_bound<mx, 29, T>(),
            get_digit_bound<mx, 30, T>(),
            get_digit_bound<mx, 31, T>(),
            get_digit_bound<mx, 32, T>(),
            get_digit_bound<mx, 33, T>(),
            get_digit_bound<mx, 34, T>(),
            get_digit_bound<mx, 35, T>(),
            get_digit_bound<mx, 36, T>(),
        };
    }  // namespace number
}  // namespace utils
