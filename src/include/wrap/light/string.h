/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// string - wrap default string type
#pragma once

#if !defined(FUTILS_WRAP_STRING_TYPE) || !defined(FUTILS_WRAP_U16STRING_TYPE) || !defined(FUTILS_WRAP_U32STRING_TYPE) || !defined(FUTILS_WRAP_PATHSTRING_TYPE)
#include <string>
#include <platform/detect.h>
#ifndef FUTILS_WRAP_STRING_TYPE
#define FUTILS_WRAP_STRING_TYPE std::string
#endif
#ifndef FUTILS_WRAP_U16STRING_TYPE
#define FUTILS_WRAP_U16STRING_TYPE std::u16string
#endif
#ifndef FUTILS_WRAP_U32STRING_TYPE
#define FUTILS_WRAP_U32STRING_TYPE std::u32string
#ifndef FUTILS_WRAP_PATHSTRING_TYPE
#ifdef FUTILS_PLATFORM_WINDOWS
#define FUTILS_WRAP_PATHSTRING_TYPE std::wstring
#else
#define FUTILS_WRAP_PATHSTRING_TYPE std::string
#endif
#endif
#endif
#endif

namespace futils {
    namespace wrap {
        using string = FUTILS_WRAP_STRING_TYPE;
        using u16string = FUTILS_WRAP_U16STRING_TYPE;
        using u32string = FUTILS_WRAP_U32STRING_TYPE;
        using path_string = FUTILS_WRAP_PATHSTRING_TYPE;
    }  // namespace wrap
}  // namespace futils
