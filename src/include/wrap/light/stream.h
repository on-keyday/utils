/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// stream - wrap std stream
#pragma once

#include <iosfwd>
#include <platform/detect.h>
namespace utils {
    namespace wrap {
#ifdef UTILS_PLATFORM_WINDOWS
        using ostream = std::wostream;
        using istream = std::wistream;
        using stringstream = std::wstringstream;
#else
        using ostream = std::ostream;
        using istream = std::istream;
        using stringstream = std::stringstream;
#endif
    }  // namespace wrap
}  // namespace utils
