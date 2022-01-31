/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// insert_space - space of str
#pragma once
#include <limits>
#include "radix.h"

namespace utils {
    namespace number {
        template <class Result, class T, class Char = char>
        constexpr NumErr insert_space(Result& result, size_t count, T input, int radix = 10, Char c = ' ') {
            if (!acceptable_radix(radix)) {
                return NumError::invalid;
            }
            std::make_unsigned_t<T> calc;
            if (input < 0) {
                calc = -input;
            }
            else {
                calc = input;
            }
            auto& v = digit_bound<T>[radix];
            size_t digit = 0;
            while (calc > v[digit]) {
                digit++;
            }
            digit++;
            if (count <= digit) {
                count = 0;
            }
            else {
                count = count - digit;
            }
            for (size_t i = 0; i < count; i++) {
                result.push_back(c);
            }
            return true;
        }
    }  // namespace number
}  // namespace utils
