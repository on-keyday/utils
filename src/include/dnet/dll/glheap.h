/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dllh.h"
#include "../heap.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <cerrno>
#endif
namespace utils {
    namespace dnet {
#ifdef _WIN32
        inline void* simple_heap_alloc(size_t size) {
            return HeapAlloc(GetProcessHeap(), 0, size);
        }

        inline void* simple_heap_realloc(void* p, size_t size) {
            return HeapReAlloc(GetProcessHeap(), 0, p, size);
        }
        inline void simple_heap_free(void* p) {
            HeapFree(GetProcessHeap(), 0, p);
        }
#else
        inline void* simple_heap_alloc(size_t size) {
            return malloc(size);
        }

        inline void* simple_heap_realloc(void* p, size_t size) {
            if (size == 0) {
                return nullptr;
            }
            return realloc(p, size);
        }

        inline void simple_heap_free(void* p) {
            free(p);
        }
#endif

        Allocs* get_alloc();

        template <class T>
        T* new_from_global_heap() {
            auto ptr = get_alloc()->alloc_ptr(sizeof(T));
            if (!ptr) {
                return nullptr;
            }
            return new (ptr) T{};
        }

        inline void* get_rawbuf(size_t sz) {
            return get_alloc()->alloc_ptr(sz);
        }

        inline void free_rawbuf(void* p) {
            get_alloc()->free_ptr(p);
        }

        inline char* get_cvec(size_t sz) {
            return static_cast<char*>(get_rawbuf(sz));
        }

        inline bool resize_cvec(char*& p, size_t size) {
            auto c = get_alloc()->realloc_ptr(p, size);
            if (c != nullptr) {
                p = static_cast<char*>(c);
            }
            return c != nullptr;
        }

        inline void free_cvec(char* p) {
            free_rawbuf(p);
        }

        template <class T>
        void delete_with_global_heap(T* p) {
            if (!p) {
                return;
            }
            p->~T();
            get_alloc()->free_ptr(p);
        }

        inline auto get_error() {
#ifdef _WIN32
            return GetLastError();
#else
            return errno;
#endif
        }

        void set_error(auto err) {
#ifdef _WIN32
            SetLastError(err);
#else
            errno = err;
#endif
        }
    }  // namespace dnet
}  // namespace utils
