/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// sized - size limited view
#pragma once
#include <cstddef>

namespace futils {
    namespace view {
        template <class T>
        struct SizedView {
            T& ref;
            size_t size_;
            constexpr SizedView(T& t, size_t sz)
                : ref(t), size_(sz) {}

            constexpr auto& operator[](size_t pos) const {
                return ref[pos];
            }

            constexpr size_t size() const {
                return size_;
            }
        };
    }  // namespace view
}  // namespace futils
