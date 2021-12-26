/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// io_completetion_port
#pragma once

namespace utils {
    namespace platform {
        namespace windows {
            void start_iocp();
            void set_handle(void* handle);
        }  // namespace windows

    }  // namespace platform
}  // namespace utils
