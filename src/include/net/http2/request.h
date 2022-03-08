/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// request - http2 request
#pragma once
#include "conn.h"
#include "stream.h"

namespace utils {
    namespace net {
        namespace http2 {
            async::Future<bool> request(AsyncIOClose&& io, Connection& conn, Header&& h);
        }
    }  // namespace net
}  // namespace utils
