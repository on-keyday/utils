/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <set>
#include "../dll/allocator.h"

namespace futils {
    namespace fnet::slib {
        template <class K>
        using set = std::set<K, std::less<K>, glheap_allocator<K>>;
    }
}  // namespace futils
