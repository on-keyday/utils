/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dns - async task dns
#pragma once

#include "../../platform/windows/dllexport_header.h"
#include "../../async/worker.h"
#include "../dns/dns.h"

namespace utils {
    namespace net {
        DLL async::Future<wrap::shared_ptr<Address>> STDCALL query(const char* host, const char* port);
    }  // namespace net
}  // namespace utils
