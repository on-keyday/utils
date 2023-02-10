/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// errno - errno wrap
#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#endif

namespace utils {
    namespace dnet {
        inline auto get_error() {
#ifdef _WIN32
            return GetLastError();
#else
            return errno;
#endif
        }

        void set_error(auto err) {
#ifdef _WIN32
            SetLastError(err);
#else
            errno = err;
#endif
        }
    }  // namespace dnet
}  // namespace utils
