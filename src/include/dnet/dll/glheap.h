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
#include <cerrno>
#endif
namespace utils {
    namespace dnet {

        dnet_dll_export(void*) get_rawbuf(size_t sz, DebugInfo info);

        dnet_dll_export(void*) resize_rawbuf(void* p, size_t sz, DebugInfo info);

        dnet_dll_export(void) free_rawbuf(void* p, DebugInfo info);

        template <class T>
        T* new_from_global_heap(DebugInfo info) {
            auto ptr = get_rawbuf(sizeof(T), info);
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
            free_rawbuf(p, info);
        }

        inline char* get_cvec(size_t sz, DebugInfo info) {
            return static_cast<char*>(get_rawbuf(sz, info));
        }

        inline bool resize_cvec(char*& p, size_t size, DebugInfo info) {
            auto c = resize_rawbuf(p, size, info);
            if (c != nullptr) {
                p = static_cast<char*>(c);
            }
            return c != nullptr;
        }

        inline void free_cvec(char* p, DebugInfo info) {
            free_rawbuf(p, info);
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
