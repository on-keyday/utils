/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// hash_map - wrap default hash_map
#pragma once
#if !defined(UTILS_WRAP_HASH_MAP_TEMPLATE) || !defined(UTILS_WRAP_HASH_MULTIMAP_TEMPLATE)
#include <unordered_map>
#define UTILS_WRAP_HASH_MAP_TEMPLATE std::unordered_map
#define UTILS_WRAP_HASH_MULTIMAP_TEMPLATE std::unordered_multimap
#endif

namespace utils {
    namespace wrap {
        template <class Key, class Value>
        using hash_map = UTILS_WRAP_HASH_MAP_TEMPLATE<Key, Value>;
        template <class Key, class Value>
        using hash_multimap = UTILS_WRAP_HASH_MULTIMAP_TEMPLATE<Key, Value>;
    }  // namespace wrap
}  // namespace utils
