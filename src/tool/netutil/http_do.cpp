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
#include "../../include/wrap/lite/map.h"
#include "../../include/net/http/http1.h"
using namespace utils;
namespace netutil {
    struct Message {
        int id;
        wrap::internal::Pack msg;
        bool endofmsg = false;
    };

    struct Redirect {
        wrap::vector<net::URI> uris;
    };

    struct Reconnect {
        wrap::vector<net::URI> uris;
        wrap::vector<net::http::Header> h;
        int index = 0;
    };

    struct FinalResult {
        wrap::vector<net::URI> uris;
        wrap::vector<net::http::Header> h;
    };

    auto msg(int id, auto&&... args) {
        return Message{.id = id, .msg = wrap::pack(args...)};
    }

    auto msgend(int id, auto&&... args) {
        auto p = msg(id, args...);
        p.endofmsg = true;
        return p;
    }

    using msg_chan = thread::SendChan<async::Any>;

    void do_http2(async::Context& ctx, net::AsyncIOClose io, msg_chan chan, size_t id, wrap::vector<net::URI> uris) {
    }

    void do_http1(async::Context& ctx, net::AsyncIOClose io, msg_chan chan, size_t id, wrap::vector<net::URI> uris) {
        auto host = uris[0].host_port();
        wrap::vector<net::http::Header> resps;
        for (int i = 0; i < uris.size(); i++) {
            auto path = uris[i].path_query();
            auto res = std::move(AWAIT(net::http::request_async(std::move(io), host.c_str(), "GET", path.c_str(), {})));
            if (res.err != net::http::HttpError::none) {
                chan << msgend(id, "error: http request to ", host, " failed\n",
                               error_msg(res.err), "\nerrno: ", res.base_err);
                return;
            }
            io = res.resp.get_io();
            auto h = res.resp.response();
            if (auto conn = h.value("connection");
                helper::contains(conn, "close", helper::ignore_case())) {
                if (i + 1 != uris.size()) {
                    Reconnect rec;
                    rec.h = std::move(resps);
                    rec.h.push_back(std::move(h));
                    rec.uris = std::move(uris);
                    rec.index = i + 1;
                    chan << std::move(rec);
                    return;
                }
            }
            else if (h.status() >= 300 && h.status() < 400) {
                if (auto loc = h.value("location")) {
                    wrap::string locuri = loc;
                    net::URI newuri;
                    preprocese_a_uri(loc, "", locuri, newuri, uris[i]);
                }
            }
            resps.push_back(std::move(h));
        }
        chan << FinalResult{
            .uris = std::move(uris),
            .h = std::move(resps),
        };
    }

    void do_request_host(async::Context& ctx, msg_chan chan, size_t id, wrap::vector<net::URI> uris) {
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
            auto conn = ssl.conn;
            auto selected = conn->alpn_selected(nullptr);
            if (!selected || ::strncmp(selected, "http/1.1", 8)) {
                return do_http1(ctx, std::move(conn), std::move(chan), id, std::move(uris));
            }
            else if (::strncmp(selected, "h2", 2)) {
                return do_http2(ctx, std::move(conn), std::move(chan), id, std::move(uris));
            }
        }
        else {
            return do_http1(ctx, std::move(tcp.conn), std::move(chan), id, std::move(uris));
        }
    }

    int http_do(subcmd::RunContext& ctx, wrap::vector<net::URI>& uris) {
        net::set_iocompletion_thread(true);
        wrap::map<wrap::string, wrap::vector<net::URI>> hosts;
        for (auto& uri : uris) {
            hosts[uri.host].push_back(std::move(uri));
        }
        auto [w, r] = thread::make_chan<async::Any>();
        size_t id = 0;
        for (auto& host : hosts) {
            net::start(do_request_host, w, id, std::move(host.second));
            id++;
        }
        size_t exists = id;
        async::Any event;
        while (r >> event) {
            if (auto msg = event.type_assert<Message>()) {
                cout << "#" << msg->id << "\n";
                cout << std::move(msg->msg);
                if (msg->endofmsg) {
                    exists--;
                }
            }
            else if (auto redirect = event.type_assert<Redirect>()) {
            }
        }
        return 0;
    }
}  // namespace netutil
