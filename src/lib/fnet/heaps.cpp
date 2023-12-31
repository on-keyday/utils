/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/heap.h>
#include <fnet/dll/glheap.h>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <cstdlib>
#endif

namespace utils {
    namespace fnet {
#ifdef UTILS_PLATFORM_WINDOWS
        void* simple_heap_alloc(void*, size_t size, size_t, DebugInfo*) {
            return HeapAlloc(GetProcessHeap(), 0, size);
        }

        void* simple_heap_realloc(void*, void* p, size_t size, size_t, DebugInfo*) {
            return HeapReAlloc(GetProcessHeap(), 0, p, size);
        }
        void simple_heap_free(void*, void* p, DebugInfo*) {
            HeapFree(GetProcessHeap(), 0, p);
        }
#else
        void* simple_heap_alloc(void*, size_t size, size_t, DebugInfo*) {
            return malloc(size);
        }

        void* simple_heap_realloc(void*, void* p, size_t size, size_t, DebugInfo*) {
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
        fnet_dll_implement(void) set_normal_allocs(Allocs set) {
            normal_hold = set;
        }

        fnet_dll_implement(void) set_objpool_allocs(Allocs set) {
            objpool_hold = set;
        }

        void* (*mem_exhausted_fn)(DebugInfo);

        fnet_dll_implement(void) set_memory_exhausted_traits(void* (*fn)(DebugInfo)) {
            mem_exhausted_fn = fn;
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

        inline void* do_alloc(Allocs* a, size_t sz, size_t align, DebugInfo info) {
            return a->alloc_ptr(a->ctx, sz, align, &info);
        }

        inline void* do_realloc(Allocs* a, void* p, size_t sz, size_t align, DebugInfo info) {
            return a->realloc_ptr(a->ctx, p, sz, align, &info);
        }

        inline void do_free(Allocs* a, void* p, DebugInfo info) {
            a->free_ptr(a->ctx, p, &info);
        }

        fnet_dll_implement(void*) alloc_normal(size_t sz, size_t align, DebugInfo info) {
            return do_alloc(get_normal_alloc(), sz, align, info);
        }

        fnet_dll_implement(void*) realloc_normal(void* p, size_t sz, size_t align, DebugInfo info) {
            return do_realloc(get_normal_alloc(), p, sz, align, info);
        }

        fnet_dll_implement(void) free_normal(void* p, DebugInfo info) {
            return do_free(get_normal_alloc(), p, info);
        }

        fnet_dll_implement(void*) alloc_objpool(size_t sz, size_t align, DebugInfo info) {
            return do_alloc(get_objpool_alloc(), sz, align, info);
        }

        fnet_dll_implement(void*) realloc_objpool(void* p, size_t sz, size_t align, DebugInfo info) {
            return do_realloc(get_objpool_alloc(), p, sz, align, info);
        }

        fnet_dll_implement(void) free_objpool(void* p, DebugInfo info) {
            return do_free(get_objpool_alloc(), p, info);
        }

        fnet_dll_implement(void*) memory_exhausted_traits(DebugInfo info) {
            auto fn = mem_exhausted_fn;
            if (fn) {
                return fn(info);
            }
            return nullptr;
        }
    }  // namespace fnet
}  // namespace utils
