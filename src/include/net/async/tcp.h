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
#include "../../wrap/lite/enum.h"

namespace utils {
    namespace net {
        enum class ConnError {
            none,
            dns_query,
            tcp_connect,
        };

        BEGIN_ENUM_STRING_MSG(ConnError, error_msg)
        ENUM_STRING_MSG(ConnError::dns_query, "dns query failed")
        ENUM_STRING_MSG(ConnError::tcp_connect, "tcp connect failed")
        END_ENUM_STRING_MSG(nullptr)

        struct ConnResult {
            ConnError err = {};
            AddrError addrerr = {};
            int errcode = 0;
            wrap::shared_ptr<TCPConn> conn;
        };

        DLL async::Future<ConnResult> STDCALL open_async(wrap::shared_ptr<Address>&& addr);
        DLL async::Future<ConnResult> STDCALL open_async(const char* host, const char* port, time_t timeout_sec = 60,
                                                         int address_family = 0, int socket_type = 0, int protocol = 0, int flags = 0);

    }  // namespace net
}  // namespace utils
