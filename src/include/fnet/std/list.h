/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <list>
#include "../dll/allocator.h"

namespace futils {
    // standard library container
    namespace fnet::slib {
        template <class T>
        using list = std::list<T, glheap_objpool_allocator<T>>;
    }
}  // namespace futils
