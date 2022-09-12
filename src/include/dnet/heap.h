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
        struct Allocs {
            void* (*alloc_ptr)(size_t);
            void* (*realloc_ptr)(void*, size_t);
            void (*free_ptr)(void*);
        };
        dnet_dll_export(void) set_alloc(Allocs alloc);
    }  // namespace dnet
}  // namespace utils
