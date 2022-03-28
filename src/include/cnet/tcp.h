/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp cnet interface
#pragma once
#include "../platform/windows/dllexport_header.h"
#include "cnet.h"

namespace utils {
    namespace cnet {
        namespace tcp {

            // create tcp client
            DLL CNet* STDCALL create_client();

            enum TCPConfig {
                ipver = user_defined_start + 1,  // set ip version
            };

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
