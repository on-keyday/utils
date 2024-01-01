/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// reverse - reverse view
#pragma once
#include "../core/buffer.h"

namespace futils {
    namespace view {
        template <class T>
        struct ReverseView {
            Buffer<buffer_t<T>> buf;

            template <class... In>
            constexpr ReverseView(In&&... in)
                : buf(std::forward<In>(in)...) {}

            using char_type = typename Buffer<buffer_t<T>>::char_type;

            constexpr char_type operator[](size_t pos) const {
                return buf.at(buf.size() - pos - 1);
            }

            constexpr size_t size() const {
                return buf.size();
            }
        };

    }  // namespace view
}  // namespace futils
