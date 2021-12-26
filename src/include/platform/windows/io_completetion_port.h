/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// io_completetion_port
#pragma once

#include "callback.h"

namespace utils {
    namespace platform {
        namespace windows {
            struct IOCPContext;
            struct IOCPObject {
                void register_handler(Complete& cmp);

               private:
                IOCPContext* ctx;
            };
            IOCPObject* start_iocp();

        }  // namespace windows

    }  // namespace platform
}  // namespace utils
