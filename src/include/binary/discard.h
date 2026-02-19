/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "writer.h"

namespace futils::binary {
    template <class C = byte>
    constexpr WriteStreamHandlerT<C, size_t> discard{
        .base = {
            .full = [](void*, size_t) { return false; },
            .direct_write =
                [](void* ptr, view::basic_rvec<C> n, size_t times) {
                    if (ptr) {
                        size_t* count = static_cast<size_t*>(ptr);
                        *count += n.size() * times;
                    }
                    return true;
                },
        },
    };
}  // namespace futils::binary