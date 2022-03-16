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
        size_t id;
        wrap::internal::Pack msg;
        bool endofmsg = false;
    };

    struct Redirect {
        size_t id;
        net::URI location;
    };

    struct Reconnect {
        wrap::vector<net::URI> uris;
        wrap::vector<net::http::Header> h;
        size_t index = 0;
        size_t id;
    };

    struct FinalResult {
        wrap::vector<net::URI> uris;
        wrap::vector<net::http::Header> h;
        size_t id;
    };

    auto msg(size_t id, auto&&... args) {
        return Message{.id = id, .msg = wrap::pack(args...)};
    }

    auto msgend(size_t id, auto&&... args) {
        auto p = msg(id, args...);
        p.endofmsg = true;
        return p;
    }

    using msg_chan = thread::SendChan<async::Any>;

    void do_http2(async::Context& ctx, net::AsyncIOClose io, msg_chan chan, size_t id, wrap::vector<net::URI> uris) {
    }

    void do_http1(async::Context& ctx, net::AsyncIOClose io, msg_chan chan, size_t id, wrap::vector<net::URI> uris, size_t start_index, wrap::vector<net::http::Header> prevhandled) {
        auto host = uris[0].host_port();
        auto scheme = uris[0].scheme;
        wrap::vector<net::http::Header> resps = std::move(prevhandled);
        for (size_t i = start_index; i < uris.size(); i++) {
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
                    rec.id = id;
                    chan << std::move(rec);
                    return;
                }
            }
            else if (h.status() >= 300 && h.status() < 400) {
                if (auto loc = h.value("location")) {
                    wrap::string locuri = loc;
                    net::URI newuri;
                    if (!preprocese_a_uri(loc, "", locuri, newuri, uris[i])) {
                        chan << msg(id, "warning: failed to parse location header\n",
                                    "location: ", loc, "\n");
                        goto END;
                    }
                    if (newuri.host == host && newuri.scheme == scheme) {
                        chan << msg(id, "redirect to ", newuri.to_string(), "\n");
                        uris.push_back(std::move(newuri));
                    }
                    else {
                        chan << Redirect{.id = id, .location = std::move(newuri)};
                    }
                }
            }
        END:
            resps.push_back(std::move(h));
        }
        chan << FinalResult{
            .uris = std::move(uris),
            .h = std::move(resps),
            .id = id,
        };
    }

    void do_request_host(async::Context& ctx, msg_chan chan, size_t id, wrap::vector<net::URI> uris, size_t procindex, wrap::vector<net::http::Header> prevproced = {}) {
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
                return do_http1(ctx, std::move(conn), std::move(chan), id, std::move(uris), procindex, std::move(prevproced));
            }
            else if (::strncmp(selected, "h2", 2)) {
                return do_http2(ctx, std::move(conn), std::move(chan), id, std::move(uris));
            }
        }
        else {
            return do_http1(ctx, std::move(tcp.conn), std::move(chan), id, std::move(uris), procindex, std::move(prevproced));
        }
    }

    int http_do(subcmd::RunContext& ctx, wrap::vector<net::URI>& uris) {
        net::set_iocompletion_thread(true);
        wrap::map<wrap::string, wrap::map<wrap::string, wrap::vector<net::URI>>> hosts;
        for (auto& uri : uris) {
            hosts[uri.host][uri.scheme].push_back(std::move(uri));
        }
        auto [w, r] = thread::make_chan<async::Any>();
        size_t id = 0;
        for (auto& host : hosts) {
            for (auto& scheme : host.second) {
                net::start(do_request_host, w, id, std::move(scheme.second), 0, wrap::vector<net::http::Header>{});
                id++;
            }
        }
        hosts.clear();
        size_t exists = id;
        async::Any event;
        wrap::vector<net::URI> processed_uri;
        wrap::vector<net::http::Header> processed_header;
        while (r >> event) {
            if (auto msg = event.type_assert<Message>()) {
                cout << "#" << msg->id << "\n";
                cout << std::move(msg->msg);
                if (msg->endofmsg) {
                    exists--;
                }
            }
            else if (auto redirect = event.type_assert<Redirect>()) {
                cout << "#" << redirect->id << "\n";
                cout << "request redirect to " << redirect->location.to_string() << "\n";
                cout << "ownership of this url has moved\n";
                hosts[redirect->location.host][redirect->location.scheme].push_back(std::move(redirect->location));
            }
            else if (auto reconnect = event.type_assert<Reconnect>()) {
                cout << "#" << reconnect->id << "\n";
                cout << "`Connection: close` has been detected\n";
                cout << "retrying to connect...\n";
                net::start(do_request_host, w, reconnect->id, std::move(reconnect->uris), reconnect->index, std::move(reconnect->h));
            }
            else if (auto final = event.type_assert<FinalResult>()) {
                cout << "#" << final->id << "\n";
                cout << "request task done\n";
                exists--;
                if (exists) {
                    cout << "waiting for others...\n";
                }
                std::move(final->uris.begin(), final->uris.end(), std::back_inserter(processed_uri));
                std::move(final->h.begin(), final->h.end(), std::back_inserter(processed_header));
            }
        }
        return 0;
    }
}  // namespace netutil
