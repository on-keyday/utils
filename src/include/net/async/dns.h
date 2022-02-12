/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "../../async/worker.h"

namespace utils {
    namespace net {
        struct DNSResult {
        };

        async::Future<DNSResult> query(const char* host, const char* port);
    }  // namespace net
}  // namespace utils
