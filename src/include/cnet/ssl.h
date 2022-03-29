/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl - cnet ssl context
#pragma once
#include "cnet.h"

namespace utils {
    namespace cnet {
        namespace ssl {
            DLL CNet* STDCALL create_client();

            enum TLSConfig {
                alpn,
                host,
                cert_file,
            };
        }  // namespace ssl
    }      // namespace cnet
}  // namespace utils
