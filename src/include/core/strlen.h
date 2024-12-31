/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// strlen - constexpr strlen
#pragma once
#include <cassert>
#include <cstddef>

namespace futils {
    template <class C>
    constexpr size_t strlen(C* ptr) {
        assert(ptr != nullptr);
        size_t length = 0;
        for (; ptr[length]; length++)
            ;
        return length;
    }
}  // namespace futils
