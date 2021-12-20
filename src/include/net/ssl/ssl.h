/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl - ssl wrapper
// depends on OpenSSL
#pragma once

#include "../core/iodef.h"

namespace utils {
    namespace net {
        namespace internal {
            struct SSLImpl;
        }

        struct SSLConn {
        };

        struct SSLResult {
           private:
            internal::SSLImpl* impl = nullptr;
        };

        SSLResult open(IO&& io);
    }  // namespace net
}  // namespace utils
