/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// insert_space - space of str
#pragma once
#include <limits>

namespace utils {
    namespace number {
        template <size_t sz, class T>
        struct Array {
            T buf[sz];
            size_t i = 0;
            constexpr T operator[](size_t f) const {
                return buf[f];
            }

            constexpr void push_back(T t) {
                buf[i] = t;
                i++;
            }
        };

        template <class T>
        constexpr T get_digit_(int radix, size_t d) {
        }
    }  // namespace number
}  // namespace utils