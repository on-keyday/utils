/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// init_net - initialize network
#pragma once
#include <platform/windows/dllexport_header.h>

namespace utils {
    namespace net {

        struct DLL Init {
           private:
            Init();
            friend DLL Init& STDCALL network();

           public:
            bool initialized();

            int errcode();
        };

        DLL Init& STDCALL network();

        DLL int STDCALL errcode();
    }  // namespace net
}  // namespace utils
