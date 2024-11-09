/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// array -  easy array class
#pragma once

namespace futils {
    namespace number {
        template <class T, size_t size_, bool strmode = false>
        struct Array {
            T buf[size_];
            size_t i = 0;
            constexpr T operator[](size_t f) const {
                return buf[f];
            }

            constexpr T& operator[](size_t f) {
                return buf[f];
            }

            constexpr size_t size() const {
                return i;
            }

            constexpr void push_back(T t) {
                if (strmode ? i >= size_ - 1 : i >= size_) {
                    return;
                }
                buf[i] = t;
                i++;
            }

            constexpr const T* c_str() const {
                return buf;
            }

            constexpr const T* data() const {
                return buf;
            }

            constexpr T* data() {
                return buf;
            }

            constexpr void resize(size_t n) {
                if (n >= capacity()) {
                    i = capacity();
                }
                else {
                    i = n;
                }
            }

            constexpr size_t capacity() const {
                return strmode ? size_ - 1 : size_;
            }

            constexpr const T* begin() const {
                return buf;
            }

            constexpr const T* end() const {
                return buf + i;
            }
        };
    }  // namespace number
}  // namespace futils
