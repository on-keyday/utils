/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// insert_space - space of str
#pragma once
#include <limits>
#include "radix.h"

namespace futils {
    namespace number {
        template <class Result, class T, class Char = char>
        constexpr NumErr insert_space(Result& result, size_t count, T input, int radix = 10, Char c = ' ') {
            if (!acceptable_radix(radix)) {
                return NumError::invalid;
            }
            std::make_unsigned_t<T> calc;
            int sign = 0;
            if (input < 0) {
                calc = -input;
                sign = 1;
            }
            else {
                calc = input;
            }
            auto& bound = digit_bound<T>;
            auto& v = bound[radix];
            size_t digit = 0;
            while (calc >= v[digit] && digit < v.size()) {
                digit++;
            }
            digit++;
            if (count <= digit + sign) {
                count = 0;
            }
            else {
                count = count - digit - sign;
            }
            for (size_t i = 0; i < count; i++) {
                result.push_back(c);
            }
            return true;
        }
    }  // namespace number
}  // namespace futils
