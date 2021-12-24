/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
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

        template <class Result, class T>
        constexpr NumErr to_string(Result& result, T in, int radix = 10, bool upper = false) {
            if (radix < 2 || radix > 36) {
                return NumError::invalid;
            }
            auto mx = radix_base_max<T>(radix);
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
            auto minus = 0;
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
