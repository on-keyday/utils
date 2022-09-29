/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// heap - dnet heap functions
#pragma once
#include "dll/dllh.h"
#include <cstddef>

namespace utils {
    namespace dnet {
        enum alloc_debug {
            aldbg_alloc,
            aldbg_realloc_before,
            aldbg_realloc,
            aldbg_free,
        };

        struct DebugInfo {
            bool size_known = false;
            size_t size = 0;
            const char* file = nullptr;
            int line = 0;
            const char* func = nullptr;
        };
#define DNET_DEBUG_MEMORY_LOCINFO(known, size_) \
    { .size_known = known, .size = size_, .file = __FILE__, .line = __LINE__, .func = __PRETTY_FUNCTION__ }

        struct Allocs {
            void* (*alloc_ptr)(size_t, DebugInfo*);
            void* (*realloc_ptr)(void*, size_t, DebugInfo*);
            void (*free_ptr)(void*, DebugInfo*);
        };

        dnet_dll_export(void) set_alloc(Allocs alloc);
    }  // namespace dnet
}  // namespace utils
