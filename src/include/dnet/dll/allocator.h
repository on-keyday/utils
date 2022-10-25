/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// allocator - allocator for dnet
#pragma once
#include "glheap.h"

namespace utils {
    namespace dnet {
        template <class T>
        struct glheap_allocator {
            using value_type = T;
            static T* allocate(size_t n) {
                return static_cast<T*>(alloc_normal(n * sizeof(T), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T))));
            }

            static void deallocate(T* t, size_t n) {
                return free_normal(t, DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T)));
            }
            template <class... Arg>
            constexpr glheap_allocator(Arg&&...) {}

            constexpr glheap_allocator() = default;
        };

        template <class T>
        struct glheap_objpool_allocator {
            using value_type = T;
            static T* allocate(size_t n) {
                return static_cast<T*>(alloc_objpool(n * sizeof(T), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T))));
            }

            static void deallocate(T* t, size_t n) {
                return free_objpool(t, DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T)));
            }
            template <class... Arg>
            constexpr glheap_objpool_allocator(Arg&&...) {}

            constexpr glheap_objpool_allocator() = default;
        };
    }  // namespace dnet
}  // namespace utils
