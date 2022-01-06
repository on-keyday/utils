/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_string - number to string
#pragma once

#include <limits>

#include "char_range.h"

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

        template <class Result, class T>
        constexpr NumErr to_string(Result& result, T in, int radix = 10, bool upper = false) {
            if (radix < 2 || radix > 36) {
                return NumError::invalid;
            }
            auto mx = radix_max_cache<T>[radix];
            bool first = true;
            bool sign = false;
            std::make_unsigned_t<T> calc;
            if (in < 0) {
                calc = -in;
                result.push_back('-');
            }
            else {
                calc = in;
            }
            if (calc == 0) {
                result.push_back('0');
                return true;
            }
            auto modulo = mx;
            std::make_unsigned_t<T> minus = 0;
            while (modulo) {
                auto d = (calc - minus) / modulo;
                minus += modulo * d;
                modulo /= radix;
                if (d || !first) {
                    result.push_back(to_num_char(d, upper));
                    first = false;
                }
            }
            return true;
        }

        template <class Result, class T>
        constexpr Result to_string(T in, int radix = 10, bool upper = false) {
            Result result;
            to_string(result, in, radix, upper);
            return result;
        }

    }  // namespace number
}  // namespace utils
