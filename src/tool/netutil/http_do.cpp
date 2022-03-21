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
#include "../../include/net/http2/request_methods.h"
#include "../../include/wrap/lite/set.h"
#include "../../include/testutil/timer.h"

using namespace utils;
namespace netutil {
    struct Message {
        size_t id;
        wrap::internal::Pack msg;
        bool timer = false;
        bool endofmsg = false;
    };

    struct Redirect {
        size_t id;
        UriWithTag uri;
    };

    struct Reconnect {
        wrap::vector<UriWithTag> uris;
        wrap::vector<net::http::Header> h;
        size_t index = 0;
        size_t id;
    };

    struct FinalResult {
        wrap::vector<UriWithTag> uris;
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

    auto timermsg(size_t id, auto&&... args) {
        auto p = msg(id, args...);
        p.timer = true;
        return p;
    }

    template <class Callback>
    bool handle_redirection(net::http::Header& h, UriWithTag& uri, msg_chan chan, size_t id,
                            const wrap::string& host, const wrap::string& scheme, Callback&& cb) {
        if (!any(uri.tagcmd.flag & TagFlag::redirect)) {
            return true;
        }
        if (auto loc = h.value("location")) {
            wrap::string locuri = loc;
            UriWithTag newuri;
            if (!preprocese_a_uri(loc, "", locuri, newuri, uri)) {
                chan << msg(id, "warning: failed to parse location header\n",
                            "location: ", loc, "\n");
                return true;
            }
            newuri.uri.tag = std::move(uri.uri.tag);
            newuri.tagcmd = std::move(uri.tagcmd);
            if (newuri.uri.host == host && newuri.uri.scheme == scheme) {
                chan << msg(id, "redirect to ", newuri.uri.to_string(), "\n");
                return cb(std::move(newuri));
            }
            else {
                chan << Redirect{.id = id, .uri = std::move(newuri)};
            }
        }
        return true;
    }

    void do_http2(async::Context& ctx, net::AsyncIOClose io, msg_chan chan, size_t id, wrap::vector<UriWithTag> uris, size_t start_index, wrap::vector<net::http::Header> prevhandled) {
        auto host = uris[0].uri.host_port();
        chan << msg(id, "starting http2 request on host ", host, " with scheme https\n");
        auto res = AWAIT(net::http2::open_async(std::move(io)));
        if (!res.conn) {
            chan << msgend(id, "error: negotiate http2 protocol with ", host, " failed\n", "errno: ", res.errcode, "\n");
            return;
        }
        auto settings = {std::pair{net::http2::SettingKey::enable_push, 0},
                         std::pair{net::http2::SettingKey::max_frame_size, 0xffffff}};
        auto nego = AWAIT(net::http2::negotiate(std::move(res.conn), settings));
        auto error_with_info = [&](net::http2::UpdateResult& res, auto&&... args) {
            chan << msgend(id, args...,
                           "h2error:", error_msg(res.err), "\n",
                           "detail: ", error_msg(res.detail), "\n",
                           "id: ", res.id, "\n",
                           "frame: ", frame_name(res.frame), "\n",
                           "status: ", status_name(res.status), "\n");
        };
        if (!nego.ctx) {
            error_with_info(nego.err, "error: negotiate http2 protocol settings with ", host, " failed\n");
            return;
        }
        auto h2ctx = nego.ctx;
        auto goaway = [&](net::http2::UpdateResult& res) {
            if (res.err != net::http2::H2Error::transport) {
                net::http2::send_goaway(ctx, h2ctx, (std::uint32_t)res.err);
            }
        };
        wrap::map<std::int32_t, UriWithTag> sid;
        auto send_header = [&](UriWithTag& utag) {
            auto& uri = utag.uri;
            net::http::Header h;
            h.set(":method", "GET");
            h.set(":authority", host.c_str());
            h.set(":scheme", "https");
            auto path = uri.path_query();
            h.set(":path", path.c_str());
            chan << msg(id, "start request to ", uri.to_string(), "\n");
            auto res = net::http2::send_header_async(ctx, h2ctx, std::move(h), true);
            if (res.err != net::http2::H2Error::none) {
                error_with_info(nego.err, "error: sending header of ", uri.to_string(), " failed\n");
                goaway(res);
                return false;
            }
            sid.emplace(res.id, std::move(utag));
            return true;
        };
        for (auto& uri : uris) {
            send_header(uri);
        }

        while (sid.size()) {
            auto data = net::http2::default_handling_ping_and_data(ctx, h2ctx);
            if (data.err.err != net::http2::H2Error::none || !data.frame) {
                if (auto found = sid.find(data.err.id);
                    data.err.err != net::http2::H2Error::transport && found != sid.end()) {
                    error_with_info(data.err, "error: error while recving data of ", found->second.uri.to_string(), "\n");
                }
                else {
                    error_with_info(data.err, "error: error while recieving data\n");
                }
                goaway(data.err);
                return;
            }
            auto f = data.frame;
            if (*verbose) {
                chan << msg(id, "frame recieved\n",
                            "type: ", frame_name(f->type), "\n",
                            "id: ", f->id, "\n",
                            "flag: ",
                            flag_state<wrap::string>(f->flag, f->type == net::http2::FrameType::settings || f->type == net::http2::FrameType::ping), "\n",
                            "len: ", f->len, "\n");
            }
            if (f->type == net::http2::FrameType::goaway) {
                error_with_info(data.err, "error: goaway was recieved in progress of receiving response\n");
                return;
            }

            if (f->id != 0) {
                auto st = h2ctx->state.stream(f->id);
                if (f->flag & net::http2::Flag::end_headers) {
                    if (auto len = st->peek_header("content-length")) {
                        std::size_t sz = 0;
                        number::parse_integer(len, sz);
                        if (sz) {
                            std::uint32_t v = (std::numeric_limits<std::uint32_t>::max)();
                            if (sz < v) {
                                v = sz;
                            }
                            auto err = net::http2::update_window_async(ctx, h2ctx, f->id, v);
                            if (err.err != net::http2::H2Error::none) {
                                error_with_info(err, "error: error while sending window update by content-length");
                            }
                            err = net::http2::update_window_async(ctx, h2ctx, 0, v);
                            if (err.err != net::http2::H2Error::none) {
                                error_with_info(err, "error: error while sending window update by content-length");
                            }
                        }
                    }
                }
                if (st->status() == net::http2::Status::closed) {
                    auto found = sid.find(f->id);
                    if (found != sid.end()) {
                        auto resp = st->response();
                        auto h = resp.response();
                        if (net::http::is_redirect_range(h.status())) {
                            if (!handle_redirection(
                                    h, found->second, chan, id, host, "https",
                                    [&](UriWithTag uri) {
                                        return send_header(uri);
                                    })) {
                                return;
                            }
                        }
                        prevhandled.push_back(std::move(h));
                        uris.push_back(std::move(found->second));
                        sid.erase(f->id);
                    }
                }
            }
        }
        chan << FinalResult{
            .uris = std::move(uris),
            .h = std::move(prevhandled),
            .id = id,
        };
    }

    void do_http1(async::Context& ctx, net::AsyncIOClose io, msg_chan chan, size_t id, wrap::vector<UriWithTag> uris, size_t start_index, wrap::vector<net::http::Header> prevhandled) {
        auto host = uris[0].uri.host_port();
        auto scheme = uris[0].uri.scheme;
        if (*verbose) {
            chan << msg(id, "starting http1 request on host ", host, " with scheme ", scheme, "\n");
        }
        wrap::vector<net::http::Header> resps = std::move(prevhandled);
        for (size_t i = start_index; i < uris.size(); i++) {
            auto path = uris[i].uri.path_query();
            if (*verbose) {
                chan << msg(id, "start request to ", uris[i].uri.to_string(), "\n");
            }
            test::Timer t;
            auto res = std::move(net::http::request_async(ctx, std::move(io), host.c_str(), "GET", path.c_str(), {}));
            auto d = t.delta();
            if (res.err != net::http::HttpError::none) {
                chan << msgend(id, "error: http request to ", host, " failed\n",
                               error_msg(res.err), "\nerrno: ", res.base_err, "\n");
                return;
            }
            if (*show_timer) {
                chan << timermsg(id, uris[i].uri.to_string(), " http1 request delta ", d.count(), "ms\n");
            }
            io = res.resp.get_io();
            auto h = res.resp.response();
            if (net::http::is_redirect_range(h.status())) {
                handle_redirection(h, uris[i], chan, id, host, scheme, [&](UriWithTag uri) {
                    uris.push_back(std::move(uri));
                    return true;
                });
            }
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
        END:
            resps.push_back(std::move(h));
        }
        chan << FinalResult{
            .uris = std::move(uris),
            .h = std::move(resps),
            .id = id,
        };
    }

    void do_request_host(async::Context& ctx, msg_chan chan, size_t id, wrap::vector<UriWithTag> uris, size_t procindex, wrap::vector<net::http::Header> prevproced = {}) {
        auto& uri = uris[0].uri;
        const char* port;
        if (uri.port.size()) {
            port = uri.port.c_str();
        }
        else {
            port = uri.scheme.c_str();
        }
        if (*verbose) {
            chan << msg(id, "starting connect to ", uri.host_port(), "\n");
        }
        auto ipver = net::afinet(uris[0].tagcmd.ipver);
        test::Timer delta;
        auto tcp = net::open_async(ctx, uri.host.c_str(), port, 60, ipver);
        if (*show_timer) {
            chan << timermsg(id, uri.host_port(), " tcp connection delta ", delta.delta().count(), "ms\n");
        }
        if (!tcp.conn) {
            chan << msgend(
                id,
                "error: open connection to `", uri.host_port(), "` failed\n", error_msg(tcp.err), "\n",
                "errno: ", tcp.errcode, "\n",
                "addrerr: ", error_msg(tcp.addrerr), "\n");
            return;
        }
        if (*verbose) {
            wrap::string ipaddr;
            tcp.conn->address()->stringify(&ipaddr);
            chan << msg(id, "remote address is ", ipaddr, "\n");
        }
        if (uri.scheme == "https") {
            if (*verbose) {
                chan << msg(id, "starting ssl negoitiation\n");
            }
            const char* alpn = *h2proto ? "\x02h2\x08http/1.1" : "\x08http/1.1";
            delta.reset();
            auto ssl = net::open_async(ctx, std::move(tcp.conn), cacert->c_str(), alpn, uri.host.c_str());
            if (!ssl.conn) {
                chan << msgend(
                    id,
                    "error: open ssl connection to `", uri.host_port(), "` failed\n", error_msg(ssl.err), "\n",
                    "sslerror: ", ssl.errcode, "\n",
                    "errno:", ssl.transporterr, "\n");
                return;
            }
            if (*show_timer) {
                chan << timermsg(id, uri.host_port(), " ssl connection delta ", delta.delta().count(), "ms\n");
            }
            auto conn = ssl.conn;
            auto selected = conn->alpn_selected(nullptr);
            if (!selected || ::strncmp(selected, "http/1.1", 8) == 0) {
                return do_http1(ctx, std::move(conn), std::move(chan), id, std::move(uris), procindex, std::move(prevproced));
            }
            else if (::strncmp(selected, "h2", 2) == 0) {
                return do_http2(ctx, std::move(conn), std::move(chan), id, std::move(uris), procindex, std::move(prevproced));
            }
            else {
                chan << msgend(id, "error: server returned invalid alpn string\n");
            }
        }
        else {
            return do_http1(ctx, std::move(tcp.conn), std::move(chan), id, std::move(uris), procindex, std::move(prevproced));
        }
    }

    test::Timer timer;

    int http_do(subcmd::RunCommand& ctx, wrap::vector<UriWithTag>& uris) {
        net::set_iocompletion_thread(true, true);
        net::get_pool().set_maxthread(std::thread::hardware_concurrency());
        net::get_pool().force_run_max_thread();
        net::get_pool().set_yield(true);
        timer.reset();
        wrap::map<wrap::string, wrap::map<wrap::string, wrap::vector<UriWithTag>>> hosts;
        auto [w, r] = thread::make_chan<async::Any>();
        size_t id = 0;
        r.set_blocking(true);
        for (auto& uri : uris) {
            hosts[uri.uri.host][uri.uri.scheme].push_back(std::move(uri));
        }
        wrap::vector<UriWithTag> processed_uri;
        wrap::vector<net::http::Header> processed_header;
        while (hosts.size()) {
            size_t exists = 0;
            for (auto& host : hosts) {
                for (auto& scheme : host.second) {
                    net::start(do_request_host, w, id, std::move(scheme.second), 0, wrap::vector<net::http::Header>{});
                    id++;
                    exists++;
                }
            }
            hosts.clear();
            async::Any event;
            while (r >> event) {
                if (auto msg = event.type_assert<Message>()) {
                    if (*verbose && !msg->timer) {
                        cout << "#" << msg->id << "\n";
                        cout << std::move(msg->msg);
                    }
                    if (msg->endofmsg) {
                        exists--;
                    }
                    if (*show_timer) {
                        if (msg->timer) {
                            cout << std::move(msg->msg);
                        }
                        cout << "message id " << msg->id;
                    }
                }
                else if (auto redirect = event.type_assert<Redirect>()) {
                    if (*verbose) {
                        cout << "#" << redirect->id << "\n";
                        cout << "request redirect to " << redirect->uri.uri.to_string() << "\n";
                        cout << "ownership of this url has moved\n";
                    }
                    hosts[redirect->uri.uri.host][redirect->uri.uri.scheme].push_back(std::move(redirect->uri));
                    if (*show_timer) {
                        cout << "redirect id " << redirect->id;
                    }
                }
                else if (auto reconnect = event.type_assert<Reconnect>()) {
                    if (*verbose) {
                        cout << "#" << reconnect->id << "\n";
                        cout << "`Connection: close` has been detected\n";
                        cout << "trying to reconnect...\n";
                    }
                    net::start(do_request_host, w, reconnect->id, std::move(reconnect->uris), reconnect->index, std::move(reconnect->h));
                    if (*show_timer) {
                        cout << "reconnect id " << reconnect->id;
                    }
                }
                else if (auto final = event.type_assert<FinalResult>()) {
                    exists--;
                    if (*verbose) {
                        cout << "#" << final->id << "\n";
                        cout << "request task done\n";
                        if (exists) {
                            cout << "waiting for others...\n";
                        }
                    }
                    std::move(final->uris.begin(), final->uris.end(), std::back_inserter(processed_uri));
                    std::move(final->h.begin(), final->h.end(), std::back_inserter(processed_header));
                    if (*show_timer) {
                        cout << "result id " << final->id;
                    }
                }
                if (*show_timer) {
                    cout << " msg delta: " << timer.delta().count() << "ms\n";
                }
                if (!exists) {
                    break;
                }
            }
        }
        for (size_t i = 0; i < processed_uri.size(); i++) {
            auto v = processed_uri[i].uri.to_string();
            if (v == "" || v == "//") {
                processed_uri.erase(processed_uri.begin() + i);
                i--;
                continue;
            }
            /*
            cout << v << "\n"
                 << processed_header[i].response();
            */
        }
        if (*verbose) {
            cout << "all tasks done\n";
        }
        if (*show_timer) {
            cout << "delta time: " << timer.delta().count() << "ms\n";
        }
        return 0;
    }
}  // namespace netutil
