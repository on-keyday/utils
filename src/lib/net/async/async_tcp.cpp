/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/async/tcp.h"
#include "../../../include/net/async/pool.h"

namespace utils {
    namespace net {
        async::Future<wrap::shared_ptr<TCPConn>> STDCALL async_open(wrap::shared_ptr<Address>&& addr) {
            if (!addr) {
                return nullptr;
            }
            return get_pool().start<wrap::shared_ptr<TCPConn>>([a = std::move(addr)](async::Context& ctx) mutable {
                auto res = open(std::move(a));
                auto p = res.connect();
                if (p) {
                    ctx.set_value(std::move(p));
                    return;
                }
                while (!p) {
                    if (res.failed()) {
                        return;
                    }
                    ctx.suspend();
                    p = res.connect();
                }
                ctx.set_value(std::move(p));
            });
        }

        async::Future<wrap::shared_ptr<TCPConn>> STDCALL async_open(const char* host, const char* port, time_t timeout_sec,
                                                                    int address_family, int socket_type, int protocol, int flags) {
            auto addr = query(host, port, timeout_sec, address_family, socket_type, protocol, flags);
            if (addr.state() == async::TaskState::invalid) {
                return nullptr;
            }
            return get_pool().start([a = std::move(addr)](async::Context& ctx) mutable {
                a.wait_or_suspend(ctx);
                auto addr = std::move(a.get());
                if (!addr) {
                    return;
                }
                auto p = async_open(std::move(addr));
                p.wait_or_suspend(ctx);
                ctx.set_value(std::move(p.get()));
                get_pool().reduce_thread();
            });
        }
    }  // namespace net
}  // namespace utils