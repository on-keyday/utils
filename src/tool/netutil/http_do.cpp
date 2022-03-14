/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net/async/tcp.h"
#include "../../include/net/async/pool.h"
#include "../../include/async/async_macro.h"
#include "../../include/thread/channel.h"
using namespace utils;
namespace netutil {
    struct Message {
        int id;
        wrap::internal::Pack msg;
    };

    auto msg(int id, auto&&... args) {
        return Message{.id = id, .msg = wrap::pack(args...)};
    }

    using msg_chan = thread::SendChan<Message>;

    int do_request_host(async::Context& ctx, msg_chan chan, int id, wrap::vector<net::URI>& uris) {
        auto& uri = uris[0];
        const char* port;
        if (uri.port.size()) {
            port = uri.port.c_str();
        }
        else {
            port = uri.scheme.c_str();
        }
        auto tcpconn = AWAIT(net::open_async(uri.host.c_str(), port));
        if (tcpconn.err != net::ConnError::none) {
            chan << msg(id, "error: open connection to `", uri.host_port(), "` failed\n");
            return false;
        }
    }
}  // namespace netutil
