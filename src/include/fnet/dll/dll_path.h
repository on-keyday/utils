/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/detect.h>
namespace futils {
    namespace fnet::lazy {
#ifdef FUTILS_PLATFORM_WINDOWS
        using dll_path = const wchar_t*;
#define fnet_lazy_dll_path(path) (L##path)
#else
        using dll_path = const char*;
#define fnet_lazy_dll_path(path) (path)
#endif

    }  // namespace fnet::lazy
}  // namespace futils
