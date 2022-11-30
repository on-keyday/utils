/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/async/ssl.h"
#include "../../../include/net/async/pool.h"

namespace utils {
    namespace net {
        async::Future<wrap::shared_ptr<SSLConn>> STDCALL async_sslopen(IO&& io, const char* cert, const char* alpn, const char* host,
                                                                       const char* selfcert, const char* selfprivate) {
            auto res = open(std::move(io), cert, alpn, host, selfcert, selfprivate);
            if (res.failed()) {
                return nullptr;
            }
            return get_pool().start<wrap::shared_ptr<SSLConn>>([r = std::move(res)](async::Context& ctx) mutable {
                while (true) {
                    auto p = r.connect();
                    if (p) {
                        ctx.set_value(std::move(p));
                        break;
                    }
                    if (r.failed()) {
                        return;
                    }
                    ctx.suspend();
                }
            });
        }
    }  // namespace net
}  // namespace utils
