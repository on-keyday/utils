/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/async/dns.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/wrap/lite/string.h"
#include "../../../include/net/core/errcode.h"
#include <thread>

namespace utils {
    namespace net {
        async::Future<AddrResult> query(const char* host, const char* port, time_t timeout_sec,
                                        int address_family, int socket_type, int protocol, int flags) {
            if (!host || !port) {
                return nullptr;
            }
            auto result = query_dns(host, port, timeout_sec, address_family, socket_type, protocol, flags);
            if (result.failed()) {
                return AddrResult{.err = AddrError::syscall_err,
                                  .errcode = errcode()};
            }
            return get_pool().start<AddrResult>([res = std::move(result)](async::Context& ctx) mutable {
                auto p = res.get_address();
                if (p) {
                    ctx.set_value(AddrResult{.addr = std::move(p)});
                    return;
                }
                while (!p) {
                    if (res.failed()) {
                        ctx.set_value(AddrResult{
                            .err = AddrError::syscall_err,
                            .errcode = errcode(),
                        });
                        return;
                    }
                    ctx.suspend();
                    std::this_thread::sleep_for(std::chrono::milliseconds(7));
                    p = res.get_address();
                }
                ctx.set_value(AddrResult{.addr = std::move(p)});
            });
        }
    }  // namespace net
}  // namespace utils
