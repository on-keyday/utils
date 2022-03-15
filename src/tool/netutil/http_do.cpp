/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net/async/tcp.h"
#include "../../include/net/async/pool.h"
#include "../../include/net/ssl/ssl.h"
#include "../../include/async/async_macro.h"
#include "../../include/thread/channel.h"
#include <execution>
using namespace utils;
namespace netutil {
    struct Message {
        int id;
        wrap::internal::Pack msg;
        bool endofmsg = false;
    };

    auto msg(int id, auto&&... args) {
        return Message{.id = id, .msg = wrap::pack(args...)};
    }

    auto msgend(int id, auto&&... args) {
        auto p = msg(id, args);
        p.endofmsg = true;
        return p;
    }

    using msg_chan = thread::SendChan<Message>;

    void do_request_host(async::Context& ctx, msg_chan chan, int id, wrap::vector<net::URI>& uris) {
        auto& uri = uris[0];
        const char* port;
        if (uri.port.size()) {
            port = uri.port.c_str();
        }
        else {
            port = uri.scheme.c_str();
        }
        auto tcp = AWAIT(net::open_async(uri.host.c_str(), port));
        if (!tcp.conn) {
            chan << msgend(
                id,
                "error: open connection to `", uri.host_port(), "` failed\n", error_msg(tcp.err), "\n",
                "errno: ", tcp.errcode, "\n",
                "addrerr: ", error_msg(tcp.addrerr), "\n");
            return;
        }
        net::AsyncIOClose io;
        if (uri.scheme == "https") {
            const char* alpn = *h2proto ? "\x02h2\x08http/1.1" : "\x08http/1.1";
            auto ssl = AWAIT(net::open_async(std::move(tcp.conn), cacert->c_str(), alpn));
            if (!ssl.conn) {
                chan << msgend(
                    id,
                    "error: open ssl connection to `", uri.host_port(), "` failed\n", error_msg(ssl.err), "\n",
                    "sslerror: ", ssl.errcode, "\n",
                    "errno:", ssl.transporterr, "\n");
                return;
            }
        }
        else {
            io = std::move(tcp.conn);
        }
    }

    int http_do(subcmd::RunContext& ctx, wrap::vector<net::URI>& uris) {
    }
}  // namespace netutil
