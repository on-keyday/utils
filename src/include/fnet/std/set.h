/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <set>
#include "../dll/allocator.h"

namespace utils {
    namespace fnet::slib {
        template <class K>
        using set = std::set<K, std::less<K>, glheap_allocator<K>>;
    }
}  // namespace utils
