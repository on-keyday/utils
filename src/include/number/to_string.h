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
        constexpr T radix_mod_zero(std::uint32_t m) {
            constexpr auto mx = (std::numeric_limits<T>::max)();
            return mx - (mx % m);
        }

        template <class Result, class T>
        constexpr NumErr to_string(Result& result, T in, int radix, bool upper = false) {
            if (radix < 2 || radix > 36) {
                return NumError::invalid;
            }
            auto modulo = radix_mod_zero(radix);
            bool first = false;
            auto calc = in;
            bool plus = false;
            if (calc < 0) {
                if (calc == (std::numeric_limits<T>::min)()) {
                    calc += 1;
                    plus = true;
                }
                calc = -calc;
            }
            while (modulo) {
                auto d = calc / modulo;
                calc = calc % modulo;
                if (d || !first) {
                    if (calc == 0 && plus) {
                        if (d + 1 >= radix) {
                        }
                    }
                    result.push_back(to_num_char(d, upper));
                    first = false;
                }
            }
        }

    }  // namespace number
}  // namespace utils
