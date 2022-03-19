/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// alloc_hook - allocator hook
#pragma once

namespace utils {
    namespace test {
#if defined(_DEBUG) && defined(_WIN32)
        bool set_log_file(const char* file);
        void set_alloc_hook(bool on);
#else
        bool set_log_file(const char* file) {
            return false;
        }
        void set_alloc_hook(bool on) {}
#endif
    }  // namespace test
}  // namespace utils
