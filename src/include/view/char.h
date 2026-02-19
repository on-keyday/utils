/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// char - char repeated view
#pragma once

namespace futils {
    namespace view {
        template <class T>
        struct CharView {
            T c[2];
            size_t size_;

            constexpr CharView(T t, size_t sz = 1)
                : c{t, 0}, size_(sz) {}

            constexpr T operator[](size_t pos) const {
                return c[0];
            }

            constexpr size_t size() const {
                return size_;
            }

            constexpr const T* c_str() const {
                return c;
            }
        };
    }  // namespace view
}  // namespace futils