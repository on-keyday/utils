/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// io_completetion_port
#pragma once

#include "dllexport_header.h"
#include "iocompletion.h"

namespace utils {
    namespace platform {
        namespace windows {

            struct IOCPContext;
            struct DLL IOCPObject {
                friend DLL IOCPObject* STDCALL start_iocp();

                bool register_handle(void* handle);

                void wait_completion(CompletionCallback cb, size_t maxcount, int wait);

               private:
                IOCPObject();
                ~IOCPObject();
                IOCPContext* ctx;
            };
            DLL IOCPObject* STDCALL start_iocp();

        }  // namespace windows

    }  // namespace platform
}  // namespace utils
