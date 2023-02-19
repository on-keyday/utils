/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// alloc_hook - allocator hook
#pragma once
#include "../platform/windows/dllexport_header.h"
#include <cstddef>

namespace utils {
    namespace test {
        enum class HookType {
            alloc,
            realloc,
            dealloc,
        };

        struct HookInfo {
            int reqid;
            size_t size;
            size_t time;
            HookType type;
        };

#if defined(_DEBUG) && defined(_WIN32)

        using Hooker = void (*)(HookInfo info);
        DLL extern Hooker log_hooker;
        DLL bool STDCALL set_log_file(const char* file);
        DLL void STDCALL set_alloc_hook(bool on);
#else

        struct fake_assign {
            constexpr const fake_assign& operator=(auto&& a) const {
                return *this;
            }
        };

        constexpr fake_assign log_hooker;

        inline bool set_log_file(const char* file) {
            return false;
        }
        inline void set_alloc_hook(bool on) {}
#endif
    }  // namespace test
}  // namespace utils
