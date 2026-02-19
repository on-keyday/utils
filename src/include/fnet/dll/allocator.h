/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// allocator - allocator for fnet
#pragma once
#include "glheap.h"

namespace futils {
    namespace fnet {
        template <class T>
        struct glheap_allocator {
            using value_type = T;
            static T* allocate(size_t n) {
                auto a = static_cast<T*>(alloc_normal(n * sizeof(T), alignof(T), DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T), alignof(T))));
                if (!a) {
                    return static_cast<T*>(memory_exhausted_traits(DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T), alignof(T))));
                }
                return a;
            }

            static void deallocate(T* t, size_t n) {
                free_normal(t, DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T), alignof(T)));
            }
            template <class... Arg>
            constexpr glheap_allocator(Arg&&...) {}

            constexpr glheap_allocator() = default;

            constexpr friend bool operator==(const glheap_allocator&, const glheap_allocator&) {
                return true;
            }
        };

        template <class T>
        struct glheap_objpool_allocator {
            using value_type = T;
            static T* allocate(size_t n) {
                auto a = static_cast<T*>(alloc_objpool(n * sizeof(T), alignof(T), DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T), alignof(T))));
                if (!a) {
                    return static_cast<T*>(memory_exhausted_traits(DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T), alignof(T))));
                }
                return a;
            }

            static void deallocate(T* t, size_t n) {
                return free_objpool(t, DNET_DEBUG_MEMORY_LOCINFO(true, n * sizeof(T), alignof(T)));
            }
            template <class... Arg>
            constexpr glheap_objpool_allocator(Arg&&...) {}

            constexpr glheap_objpool_allocator() = default;

            constexpr bool operator==(const glheap_objpool_allocator&) {
                return true;
            }
        };
    }  // namespace fnet
}  // namespace futils
