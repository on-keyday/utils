/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// io_completetion_port
#pragma once

#include "dllexport.h"
#include "callback.h"

namespace utils {
    namespace platform {
        namespace windows {
            struct IOCPContext;
            struct DLL IOCPObject {
                friend IOCPObject* start_iocp();

                bool register_handler(void* handle, Complete&& complete);

               private:
                IOCPObject();
                ~IOCPObject();
                IOCPContext* ctx;
            };
            IOCPObject* start_iocp();

        }  // namespace windows

    }  // namespace platform
}  // namespace utils
