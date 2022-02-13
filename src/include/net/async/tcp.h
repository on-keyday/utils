/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - async tcp
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "dns.h"
#include "../tcp/tcp.h"

namespace utils {
    namespace net {
        DLL async::Future<wrap::shared_ptr<TCPConn>> STDCALL async_open(wrap::shared_ptr<Address>&& addr);
    }
}  // namespace utils
