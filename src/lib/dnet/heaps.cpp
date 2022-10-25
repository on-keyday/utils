/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/glheap.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <cstdlib>
#endif

namespace utils {
    namespace dnet {
#ifdef _WIN32
        void* simple_heap_alloc(void*, size_t size, DebugInfo*) {
            return HeapAlloc(GetProcessHeap(), 0, size);
        }

        void* simple_heap_realloc(void*, void* p, size_t size, DebugInfo*) {
            return HeapReAlloc(GetProcessHeap(), 0, p, size);
        }
        void simple_heap_free(void*, void* p, DebugInfo*) {
            HeapFree(GetProcessHeap(), 0, p);
        }
#else
        void* simple_heap_alloc(void*, size_t size, DebugInfo*) {
            return malloc(size);
        }

        void* simple_heap_realloc(void*, void* p, size_t size, DebugInfo*) {
            if (size == 0) {
                return nullptr;
            }
            return realloc(p, size);
        }

        void simple_heap_free(void*, void* p, DebugInfo*) {
            free(p);
        }
#endif

        static Allocs normal_hold;
        static Allocs objpool_hold;
        dnet_dll_implement(void) set_normal_allocs(Allocs set) {
            normal_hold = set;
        }

        dnet_dll_implement(void) set_objpool_allocs(Allocs set) {
            objpool_hold = set;
        }

        Allocs get_(Allocs s) {
            if (!s.alloc_ptr || !s.realloc_ptr || !s.free_ptr) {
                s.alloc_ptr = simple_heap_alloc;
                s.realloc_ptr = simple_heap_realloc;
                s.free_ptr = simple_heap_free;
            }
            return s;
        }

        Allocs* get_normal_alloc() {
            static Allocs alloc = get_(normal_hold);
            return &alloc;
        }

        Allocs* get_objpool_alloc() {
            static Allocs alloc = get_(objpool_hold);
            return &alloc;
        }

        inline void* do_alloc(Allocs* a, size_t sz, DebugInfo info) {
            return a->alloc_ptr(a->ctx, sz, &info);
        }

        inline void* do_realloc(Allocs* a, void* p, size_t sz, DebugInfo info) {
            return a->realloc_ptr(a->ctx, p, sz, &info);
        }

        inline void do_free(Allocs* a, void* p, DebugInfo info) {
            a->free_ptr(a->ctx, p, &info);
        }

        dnet_dll_implement(void*) alloc_normal(size_t sz, DebugInfo info) {
            return do_alloc(get_normal_alloc(), sz, info);
        }

        dnet_dll_implement(void*) realloc_normal(void* p, size_t sz, DebugInfo info) {
            return do_realloc(get_normal_alloc(), p, sz, info);
        }

        dnet_dll_implement(void) free_normal(void* p, DebugInfo info) {
            return do_free(get_normal_alloc(), p, info);
        }

        dnet_dll_implement(void*) alloc_objpool(size_t sz, DebugInfo info) {
            return do_alloc(get_objpool_alloc(), sz, info);
        }

        dnet_dll_export(void*) realloc_objpool(void* p, size_t sz, DebugInfo info) {
            return do_realloc(get_objpool_alloc(), p, sz, info);
        }

        dnet_dll_export(void) free_objpool(void* p, DebugInfo info) {
            return do_free(get_objpool_alloc(), p, info);
        }
    }  // namespace dnet
}  // namespace utils
