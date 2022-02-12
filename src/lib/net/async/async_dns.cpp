/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/async/dns.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/wrap/lite/string.h"

namespace utils {
    namespace net {
        async::Future<wrap::shared_ptr<Address>> query(const char* host, const char* port) {
            if (!host || !port) {
                return nullptr;
            }
            wrap::string host_ = host, port_ = port;
            return get_pool().start<wrap::shared_ptr<Address>>([host_, port_](async::Context& ctx) {
                auto result = query_dns(host_.c_str(), port_.c_str());
                auto p = result.get_address();
                if (p) {
                    ctx.set_value(std::move(p));
                    return;
                }
                while (!p) {
                    if (result.failed()) {
                        return;
                    }
                }
            });
        }
    }  // namespace net
}  // namespace utils