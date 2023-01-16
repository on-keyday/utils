/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// array - easy array
#pragma once
#include <cstddef>

namespace utils {
    namespace dnet::slib {
        template <class T, size_t n>
        struct array {
            T buf[n];
            const T& operator[](size_t i) const {
                return buf[i];
            }

            T& operator[](size_t i) {
                return buf[i];
            }

            T* begin() {
                return buf;
            }
            T* end() {
                return buf + n;
            }

            const T* begin() const {
                return buf;
            }

            const T* end() const {
                return buf + n;
            }

            size_t size() const {
                return n;
            }
        };

    }  // namespace dnet::slib
}  // namespace utils
