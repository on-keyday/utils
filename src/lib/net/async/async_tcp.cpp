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
    }  // namespace net
}  // namespace utils
