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

#include "../../wrap/lite/smart_ptr.h"

namespace utils {
    namespace net {
        namespace internal {
            struct SSLImpl;
        }

        struct SSLConn {
            friend struct SSLResult;

            constexpr SSLConn() {}

            State write(const char* ptr, size_t size);
            State read(char* ptr, size_t size, size_t* red);
            State close(bool force = false);

            ~SSLConn();

           private:
            internal::SSLImpl* impl = nullptr;
        };

        struct SSLResult {
            friend SSLResult open(IO&& io, const char* cert, const char* alpn, const char* host,
                                  const char* selfcert, const char* selfprivate);
            constexpr SSLResult() {}

            SSLResult(SSLResult&&);

            SSLResult& operator=(SSLResult&&);

            SSLResult(const SSLResult&) = delete;

            SSLResult& operator=(const SSLResult&) = delete;

            wrap::shared_ptr<SSLConn> connect();

            bool failed();

            ~SSLResult();

           private:
            internal::SSLImpl* impl = nullptr;
        };

        struct SSLServer {
            SSLResult accept(IO&& io);

           private:
            internal::SSLImpl* impl = nullptr;
        };

        SSLResult open(IO&& io, const char* cert, const char* alpn = nullptr, const char* host = nullptr,
                       const char* selfcert = nullptr, const char* selfprivate = nullptr);
        SSLServer setup();
    }  // namespace net
}  // namespace utils
