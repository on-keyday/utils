/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl - async ssl connection
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "../ssl/ssl.h"
#include "../../async/worker.h"

namespace utils {
    namespace net {
        DLL [[deprecated]] async::Future<wrap::shared_ptr<SSLConn>> async_sslopen(IO&& io, const char* cert, const char* alpn = nullptr, const char* host = nullptr,
                                                                                  const char* selfcert = nullptr, const char* selfprivate = nullptr);
    }
}  // namespace utils
