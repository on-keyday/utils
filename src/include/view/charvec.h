/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// charvec - charvec view
#pragma once
#include <cstddef>

namespace utils {
    namespace view {
        template <class T>
        struct CharVec {
            T* ptr;
            size_t size_;
            constexpr CharVec(T* t, size_t sz)
                : ptr(t), size_(sz) {}

            constexpr T& operator[](size_t pos) const {
                return ptr[pos];
            }

            constexpr size_t size() const {
                return size_;
            }
        };
    }  // namespace view
}  // namespace utils
