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
            friend SSLResult open(IO&& io, const char* cert, const char* alpn, const char* host,
                                  const char* selfcert, const char* selfprivate);
            constexpr SSLResult() {}

            SSLResult(SSLResult&&);

            SSLResult& operator=(SSLResult&&);

            std::shared_ptr<SSLConn> connect();

            bool failed();

           private:
            internal::SSLImpl* impl = nullptr;
        };

        SSLResult open(IO&& io, const char* cert, const char* alpn = nullptr, const char* host = nullptr,
                       const char* selfcert = nullptr, const char* selfprivate = nullptr);
    }  // namespace net
}  // namespace utils
