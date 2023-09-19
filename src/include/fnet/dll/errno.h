/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// errno - errno wrap
#pragma once
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <cerrno>
#endif

namespace utils {
    namespace fnet {
        inline auto get_error() {
#ifdef UTILS_PLATFORM_WINDOWS
            return GetLastError();
#else
            return errno;
#endif
        }

        void set_error(auto err) {
#ifdef UTILS_PLATFORM_WINDOWS
            SetLastError(err);
#else
            errno = err;
#endif
        }
    }  // namespace fnet
}  // namespace utils
