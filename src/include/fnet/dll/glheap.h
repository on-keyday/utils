/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dllh.h"
#include "../heap.h"

namespace utils {
    namespace fnet {

        fnet_dll_export(void*) alloc_normal(size_t size, size_t align, DebugInfo info);

        fnet_dll_export(void*) realloc_normal(void* p, size_t size, size_t align, DebugInfo info);

        fnet_dll_export(void) free_normal(void* p, DebugInfo info);

        fnet_dll_export(void*) alloc_objpool(size_t size, size_t align, DebugInfo info);

        fnet_dll_export(void*) realloc_objpool(void* p, size_t size, size_t align, DebugInfo info);

        fnet_dll_export(void) free_objpool(void* p, DebugInfo info);

        fnet_dll_export(void*) memory_exhausted_traits(DebugInfo info);

        fnet_dll_export(void) set_memory_exhausted_traits(void* (*fn)(DebugInfo));

        template <class T>
        T* new_from_global_heap(DebugInfo info) {
            auto ptr = alloc_normal(sizeof(T), alignof(T), info);
            if (!ptr) {
                return nullptr;
            }
            return new (ptr) T{};
        }

        template <class T>
        void delete_with_global_heap(T* p, DebugInfo info) {
            if (!p) {
                return;
            }
            p->~T();
            info.size = sizeof(T);
            info.size_known = true;
            free_normal(p, info);
        }

        inline char* get_charvec(size_t sz, DebugInfo info) {
            return static_cast<char*>(alloc_normal(sz, alignof(char), info));
        }

        inline bool resize_charvec(char*& p, size_t size, DebugInfo info) {
            auto c = realloc_normal(p, size, alignof(char), info);
            if (c != nullptr) {
                p = static_cast<char*>(c);
            }
            return c != nullptr;
        }

        inline void free_charvec(char* p, DebugInfo info) {
            free_normal(p, info);
        }

    }  // namespace fnet
}  // namespace utils
