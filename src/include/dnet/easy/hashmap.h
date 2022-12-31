/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hashmap - hash map
#pragma once
#include "../dll/allocator.h"
#include <unordered_map>

namespace utils {
    namespace dnet {
        namespace easy {
            /*
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
            };*/

            template <class K, class V>
            struct H {
               private:
                std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, glheap_objpool_allocator<std::pair<const K, V>>> h;

               public:
                decltype(auto) operator[](auto&& key) {
                    return h[key];
                }

                auto find(auto&& key) {
                    return h.find(key);
                }

                auto begin() {
                    return h.begin();
                }

                auto end() {
                    return h.end();
                }

                auto emplace(auto&&... args) {
                    return h.emplace(std::forward<decltype(args)>(args)...);
                }

                auto erase(auto&& key) {
                    return h.erase(key);
                }

                auto clear() {
                    return h.clear();
                }
            };
        }  // namespace easy
    }      // namespace dnet
}  // namespace utils
