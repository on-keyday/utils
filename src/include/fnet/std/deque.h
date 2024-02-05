/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// deque - std::deque wrap
#pragma once
#include <deque>
#include "../dll/allocator.h"

namespace futils {
    namespace fnet::slib {
        template <class T>
        using deque = std::deque<T, glheap_allocator<T>>;
    }
}  // namespace futils
