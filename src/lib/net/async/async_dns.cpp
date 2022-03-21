/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/async/dns.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/wrap/light/string.h"
#include "../../../include/net/core/errcode.h"
#include <thread>

namespace utils {
    namespace net {
        async::Future<AddrResult> query(const char* host, const char* port, time_t timeout_sec,
                                        int address_family, int socket_type, int protocol, int flags) {
            return net::start([=](async::Context& ctx) {
                return query(ctx, host, port, timeout_sec, address_family, socket_type, protocol, flags);
            });
        }

        DLL AddrResult STDCALL query(async::Context& ctx, const char* host, const char* port, time_t timeout_sec,
                                     int address_family, int socket_type, int protocol, int flags) {
            if (!host || !port) {
                return {.err = AddrError::invalid_arg};
            }
            auto res = query_dns(host, port, timeout_sec, address_family, socket_type, protocol, flags);
            if (res.failed()) {
                return AddrResult{.err = AddrError::syscall_err,
                                  .errcode = errcode()};
            }
            auto p = res.get_address();
            if (p) {
                return {.addr = std::move(p)};
            }
            while (!p) {
                if (res.failed()) {
                    return {
                        .err = AddrError::syscall_err,
                        .errcode = errcode(),
                    };
                }
                ctx.suspend();
                std::this_thread::sleep_for(std::chrono::milliseconds(7));
                p = res.get_address();
            }
            return {.addr = std::move(p)};
        }
    }  // namespace net
}  // namespace utils
