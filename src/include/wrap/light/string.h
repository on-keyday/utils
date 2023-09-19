/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// string - wrap default string type
#pragma once

#if !defined(UTILS_WRAP_STRING_TYPE) || !defined(UTILS_WRAP_U16STRING_TYPE) || !defined(UTILS_WRAP_U32STRING_TYPE) || !defined(UTILS_WRAP_PATHSTRING_TYPE)
#include <string>
#include <platform/detect.h>
#ifndef UTILS_WRAP_STRING_TYPE
#define UTILS_WRAP_STRING_TYPE std::string
#endif
#ifndef UTILS_WRAP_U16STRING_TYPE
#define UTILS_WRAP_U16STRING_TYPE std::u16string
#endif
#ifndef UTILS_WRAP_U32STRING_TYPE
#define UTILS_WRAP_U32STRING_TYPE std::u32string
#ifndef UTILS_WRAP_PATHSTRING_TYPE
#ifdef UTILS_PLATFORM_WINDOWS
#define UTILS_WRAP_PATHSTRING_TYPE std::wstring
#else
#define UTILS_WRAP_PATHSTRING_TYPE std::string
#endif
#endif
#endif
#endif

namespace utils {
    namespace wrap {
        using string = UTILS_WRAP_STRING_TYPE;
        using u16string = UTILS_WRAP_U16STRING_TYPE;
        using u32string = UTILS_WRAP_U32STRING_TYPE;
        using path_string = UTILS_WRAP_PATHSTRING_TYPE;
    }  // namespace wrap
}  // namespace utils
