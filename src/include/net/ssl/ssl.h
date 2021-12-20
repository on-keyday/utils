/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl - ssl wrapper
// depends on OpenSSL
#pragma once

namespace utils {
    namespace net {
        namespace internal {
            struct SSLImpl;
        }

        struct SSLResult {
        };

        SSLResult open();
    }  // namespace net
}  // namespace utils
