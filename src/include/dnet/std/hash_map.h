/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <unordered_map>
#include "../dll/allocator.h"

namespace utils {
    namespace dnet::slib {
        template <class K, class V>
        using hash_map = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, glheap_allocator<std::pair<const K, V>>>;
    }
}  // namespace utils