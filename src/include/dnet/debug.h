/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// debug - debug util
#pragma once
#include <cstdlib>
#include "heap.h"
#include "../helper/defer.h"
#ifdef _WIN32
#include <crtdbg.h>
#endif

namespace utils {
    namespace dnet::debug {

        template <class T>
        constexpr void do_callback(void* cb, int i) {
            if constexpr (!std::is_same_v<T, void>) {
                (*(T*)cb)(i);
            }
        }

        template <class T = void>
        constexpr Allocs allocs(T* cb = nullptr) {
            utils::dnet::Allocs allocs;
#ifdef _WIN32
            allocs.alloc_ptr = [](void* cb, size_t size, size_t, utils::dnet::DebugInfo* info) {
                do_callback<T>(cb, 1);
                return _malloc_dbg(size, _NORMAL_BLOCK, info->file, info->line);
            };
            allocs.realloc_ptr = [](void* cb, void* p, size_t size, size_t, utils::dnet::DebugInfo* info) {
                do_callback<T>(cb, 2);
                return _realloc_dbg(p, size, _NORMAL_BLOCK, info->file, info->line);
            };
            allocs.free_ptr = [](void* cb, void* p, utils::dnet::DebugInfo*) {
                do_callback<T>(cb, 0);
                return _free_dbg(p, _NORMAL_BLOCK);
            };
            allocs.ctx = cb;
#else
            allocs.alloc_ptr = [](void*, size_t size, size_t, utils::dnet::DebugInfo*) {
                return malloc(size);
            };
            allocs.realloc_ptr = [](void*, void* p, size_t size, size_t, utils::dnet::DebugInfo*) {
                return realloc(p, size);
            };
            allocs.free_ptr = [](void*, void* p, utils::dnet::DebugInfo*) {
                return free(p);
            };
            allocs.ctx = nullptr;
#endif
            return allocs;
        }
    }  // namespace dnet::debug
}  // namespace utils
