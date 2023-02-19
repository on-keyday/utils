/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// heap - fnet heap functions
#pragma once
#include "dll/dllh.h"
#include <cstddef>

namespace utils {
    namespace fnet {

        struct DebugInfo {
            bool size_known = false;
            size_t size = 0;
            size_t align = 0;
            const char* file = nullptr;
            int line = 0;
            const char* func = nullptr;
        };
#define DNET_DEBUG_MEMORY_LOCINFO(known, size_, align_) \
    { .size_known = known, .size = size_, .align = align_, .file = __FILE__, .line = __LINE__, .func = __PRETTY_FUNCTION__ }

        struct Allocs {
            void* ctx;
            void* (*alloc_ptr)(void* ctx, size_t, size_t, DebugInfo*);
            void* (*realloc_ptr)(void* ctx, void*, size_t, size_t, DebugInfo*);
            void (*free_ptr)(void* ctx, void*, DebugInfo*);
        };

        fnet_dll_export(void) set_normal_allocs(Allocs alloc);
        fnet_dll_export(void) set_objpool_allocs(Allocs alloc);
    }  // namespace fnet
}  // namespace utils
