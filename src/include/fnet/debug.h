/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// debug - debug util
#pragma once
#include <cstdlib>
#include "heap.h"
#include "../helper/defer.h"
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <crtdbg.h>
#endif

namespace utils {
    namespace fnet::debug {

        template <class T>
        constexpr void do_callback(void* cb, int i, DebugInfo* info) {
            if constexpr (!std::is_same_v<T, void>) {
                (*(T*)cb)(i, info);
            }
        }

        template <class T = void>
        constexpr Allocs allocs(T* cb = nullptr) {
            utils::fnet::Allocs allocs;
#ifdef UTILS_PLATFORM_WINDOWS
            allocs.alloc_ptr = [](void* cb, size_t size, size_t, utils::fnet::DebugInfo* info) {
                do_callback<T>(cb, 1, info);
                return _malloc_dbg(size, _NORMAL_BLOCK, info->file, info->line);
            };
            allocs.realloc_ptr = [](void* cb, void* p, size_t size, size_t, utils::fnet::DebugInfo* info) {
                do_callback<T>(cb, 2, info);
                return _realloc_dbg(p, size, _NORMAL_BLOCK, info->file, info->line);
            };
            allocs.free_ptr = [](void* cb, void* p, utils::fnet::DebugInfo* info) {
                do_callback<T>(cb, 0, info);
                return _free_dbg(p, _NORMAL_BLOCK);
            };
            allocs.ctx = cb;
#else
            allocs.alloc_ptr = [](void*, size_t size, size_t, utils::fnet::DebugInfo*) {
                return malloc(size);
            };
            allocs.realloc_ptr = [](void*, void* p, size_t size, size_t, utils::fnet::DebugInfo*) {
                return realloc(p, size);
            };
            allocs.free_ptr = [](void*, void* p, utils::fnet::DebugInfo*) {
                return free(p);
            };
            allocs.ctx = nullptr;
#endif
            return allocs;
        }
    }  // namespace fnet::debug
}  // namespace utils
