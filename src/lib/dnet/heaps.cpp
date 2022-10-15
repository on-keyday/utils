/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/glheap.h>
#ifndef _WIN32
#include <cstdlib>
#endif

namespace utils {
    namespace dnet {
#ifdef _WIN32
        void* simple_heap_alloc(size_t size, DebugInfo*) {
            return HeapAlloc(GetProcessHeap(), 0, size);
        }

        void* simple_heap_realloc(void* p, size_t size, DebugInfo*) {
            return HeapReAlloc(GetProcessHeap(), 0, p, size);
        }
        void simple_heap_free(void* p, DebugInfo*) {
            HeapFree(GetProcessHeap(), 0, p);
        }
#else
        void* simple_heap_alloc(size_t size, DebugInfo*) {
            return malloc(size);
        }

        void* simple_heap_realloc(void* p, size_t size, DebugInfo*) {
            if (size == 0) {
                return nullptr;
            }
            return realloc(p, size);
        }

        void simple_heap_free(void* p, DebugInfo*) {
            free(p);
        }
#endif

        static Allocs tmphold;
        void set_allocs(Allocs set) {
            tmphold = set;
        }

        Allocs get_() {
            auto s = tmphold;
            if (!s.alloc_ptr || !s.realloc_ptr || !s.free_ptr) {
                s.alloc_ptr = simple_heap_alloc;
                s.realloc_ptr = simple_heap_realloc;
                s.free_ptr = simple_heap_free;
            }
            return s;
        }

        Allocs* get_alloc() {
            static Allocs alloc = get_();
            return &alloc;
        }

        dnet_dll_implement(void*) get_rawbuf(size_t sz, DebugInfo info) {
            auto a = get_alloc();
            return a->alloc_ptr(sz, &info);
        }

        dnet_dll_implement(void*) resize_rawbuf(void* p, size_t sz, DebugInfo info) {
            auto a = get_alloc();
            return a->realloc_ptr(p, sz, &info);
        }

        dnet_dll_implement(void) free_rawbuf(void* p, DebugInfo info) {
            auto a = get_alloc();
            a->free_ptr(p, &info);
        }
    }  // namespace dnet
}  // namespace utils
