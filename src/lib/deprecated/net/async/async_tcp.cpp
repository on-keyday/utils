/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <platform/windows/dllexport_source.h>
#include <deprecated/net/async/tcp.h>
#include <deprecated/net/async/pool.h>
#include <deprecated/net/core/errcode.h>
#include <thread>

namespace utils {
    namespace net {
        ConnResult STDCALL open_async(async::Context& ctx, wrap::shared_ptr<Address>&& addr) {
            if (!addr) {
                return ConnResult{.err = ConnError::dns_query,
                                  .addrerr = AddrError::invalid_address};
            }
            auto res = open(std::move(addr));
            auto p = res.connect();
            if (p) {
                return {.conn = std::move(p)};
            }
            while (!p) {
                if (res.failed()) {
                    return {.err = ConnError::tcp_connect,
                            .errcode = errcode()};
                }
                ctx.suspend();
                std::this_thread::sleep_for(std::chrono::milliseconds(7));
                p = res.connect();
            }
            return {.conn = std::move(p)};
        }

        async::Future<ConnResult> STDCALL open_async(wrap::shared_ptr<Address>&& addr) {
            return net::start(
                [](async::Context& ctx, auto v) {
                    return open_async(ctx, std::move(v));
                },
                std::move(addr));
        }

        async::Future<ConnResult> STDCALL open_async(const char* host, const char* port, time_t timeout_sec,
                                                     int address_family, int socket_type, int protocol, int flags) {
            return net::start(
                [](async::Context& ctx, auto... args) {
                    return open_async(ctx, args...);
                },
                host, port, timeout_sec, address_family, socket_type, protocol, flags);
        }

        ConnResult STDCALL open_async(async::Context& ctx, const char* host, const char* port, time_t timeout_sec,
                                      int address_family, int socket_type, int protocol, int flags) {
            auto p = query(ctx, host, port, timeout_sec, address_family, socket_type, protocol, flags);
            if (p.err != AddrError::none) {
                return ConnResult{
                    .err = ConnError::dns_query,
                    .addrerr = p.err,
                    .errcode = p.errcode,
                };
            }
            return open_async(ctx, std::move(p.addr));
        }
    }  // namespace net
}  // namespace utils
