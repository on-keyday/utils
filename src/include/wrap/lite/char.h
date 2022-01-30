/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// char - char aliases
#pragma once

namespace utils {
    namespace wrap {
#ifdef _WIN32
        using path_char = wchar_t;
#define TO_TCHAR(c) L##c
#else
        using path_char = char;
#define TO_TCHAR(c) c
#endif
    }  // namespace wrap
}  // namespace utils