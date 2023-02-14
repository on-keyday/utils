/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace utils {
    namespace dnet::lazy {
#ifdef _WIN32
        using dll_path = const wchar_t*;
#define dnet_lazy_dll_path(path) (L##path)
#elif
        using dll_path = const char*;
#define dnet_lazy_dll_path(path) (##path)
#endif

    }  // namespace dnet::lazy
}  // namespace utils
