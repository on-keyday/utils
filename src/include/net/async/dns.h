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
#include <wrap/light/enum.h>

namespace utils {
    namespace net {
        enum class AddrError {
            none,
            invalid_arg,
            invalid_address,
            syscall_err,
        };

        BEGIN_ENUM_STRING_MSG(AddrError, error_msg)
        ENUM_STRING_MSG(AddrError::invalid_address, "invalid address")
        ENUM_STRING_MSG(AddrError::syscall_err, "syscall error")
        END_ENUM_STRING_MSG("none")

        struct AddrResult {
            AddrError err;
            wrap::shared_ptr<Address> addr;
            int errcode;
        };

        DLL async::Future<AddrResult> STDCALL query(const char* host, const char* port, time_t timeout_sec = 60,
                                                    int address_family = 0, int socket_type = 0, int protocol = 0, int flags = 0);

        DLL AddrResult STDCALL query(async::Context& ctx, const char* host, const char* port, time_t timeout_sec = 60,
                                     int address_family = 0, int socket_type = 0, int protocol = 0, int flags = 0);
    }  // namespace net
}  // namespace utils
