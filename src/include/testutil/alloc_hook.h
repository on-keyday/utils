/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// alloc_hook - allocator hook
#pragma once
#include "../platform/windows/dllexport_header.h"

namespace utils {
    namespace test {
#if defined(_DEBUG) && defined(_WIN32)
        DLL bool STDCALL set_log_file(const char* file);
        DLL void STDCALL set_alloc_hook(bool on);
#else
        inline bool set_log_file(const char* file) {
            return false;
        }
        inline void set_alloc_hook(bool on) {}
#endif
    }  // namespace test
}  // namespace utils
