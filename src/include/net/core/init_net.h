/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// init_net - initialize network
#pragma once
#include "../../platform/windows/dllexport_header.h"

namespace utils {
    namespace net {

        struct DLL Init {
           private:
            Init();
            friend Init& network();

           public:
            bool initialized();
        };

        DLL Init& STDCALL network();

    }  // namespace net
}  // namespace utils
