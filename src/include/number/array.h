/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// array -  easy array class
#pragma once

namespace utils {
    namespace number {
        template <size_t sz, class T, bool strmode = false>
        struct Array {
            T buf[sz];
            size_t i = 0;
            constexpr T operator[](size_t f) const {
                return buf[f];
            }

            constexpr size_t size() const {
                return i;
            }

            constexpr void push_back(T t) {
                if (strmode ? i >= sz - 1 : i >= sz) {
                    return;
                }
                buf[i] = t;
                i++;
            }

            constexpr const T* c_str() const {
                return buf;
            }

            constexpr size_t capacity() const {
                return strmode ? sz - 1 : sz;
            }
        };
    }  // namespace number
}  // namespace utils
