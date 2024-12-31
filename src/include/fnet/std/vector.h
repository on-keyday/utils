/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <vector>
#include "../dll/allocator.h"

namespace futils {
    namespace fnet::slib {
        // use glheap_allcator
        template <class T>
        using vector = std::vector<T, glheap_allocator<T>>;
    }  // namespace fnet::slib
}  // namespace futils
