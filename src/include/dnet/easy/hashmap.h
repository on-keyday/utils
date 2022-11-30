/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hashmap - hash map
#pragma once
#include "../dll/allocator.h"
#include "vector.h"
#include <utility>

namespace utils {
    namespace dnet {
        namespace easy {
            namespace internal {
                template <class Key, class Value>
                struct hash_link {
                    std::pair<const Key, Value> storage;
                    hash_link* next = nullptr;
                };

                template <class Key, class Value, class Alloc>
                struct hash_table {
                    vector<internal::hash_link<Key, Value>*> table;
                };
            }  // namespace internal

            template <class Key, class Value, class Alloc = glheap_allocator<std::pair<Key, Value>>>
            struct hash_map {
               private:
            };
        }  // namespace easy
    }      // namespace dnet
}  // namespace utils
