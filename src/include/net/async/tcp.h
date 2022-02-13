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
        DLL async::Future<wrap::shared_ptr<TCPConn>> STDCALL async_open(const char* host, const char* port, time_t timeout_sec = 60,
                                                                        int address_family = 0, int socket_type = 0, int protocol = 0, int flags = 0);
    }  // namespace net
}  // namespace utils
