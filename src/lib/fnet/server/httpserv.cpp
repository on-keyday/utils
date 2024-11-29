/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/server/servcpp.h>
#include <fnet/http1/http1.h>
#include <fnet/server/state.h>
#include <fnet/server/httpserv.h>
#include <wrap/cout.h>
#include <fnet/sockutil.h>

namespace futils {
    namespace fnet {
        namespace server {

            bool& response_sent(Requester& req) {
                return req.response_sent;
            }

            void http_handler_impl(HTTPServ* serv, std::shared_ptr<Transport>&& req, StateContext as);

            struct read_buffer_getter {
                std::shared_ptr<Transport> req;

                view::wvec get_buffer() {
                    return req->get_buffer();
                }

                void add_data(view::rvec d, StateContext as) {
                    req->add_data(d, as);
                }
            };

            void call_read_async(HTTPServ* serv, std::shared_ptr<Transport>& req, StateContext& as) {
                auto fn = [=](Socket&& sock, read_buffer_getter&& bg, StateContext&& as, bool was_err) {
                    auto req = bg.req;
                    if (was_err) {
                        return;  // discard
                    }
                    as.log(log_level::debug, "reenter http handler from async read", req->client.addr);
                    http_handler_impl(serv, std::move(req), std::move(as));
                };
                auto sock = req->client.sock.clone();
                if (!as.read_async(sock, read_buffer_getter{std::move(req)}, fn)) {
                    sock.shutdown();  // discard
                }
            }

            struct write_buffer_getter {
                std::shared_ptr<Transport> req;
                view::rvec buf;

                view::rvec get_buffer() {
                    return buf;
                }
            };

            void do_write(HTTPServ* serv, std::shared_ptr<Transport>&& req, view::rvec remain, StateContext& as, auto&& cb);

            void call_write_async(
                HTTPServ* serv,
                std::shared_ptr<Transport>&& req,
                view::rvec buf,
                StateContext& as, auto&& next) {
                auto fn = [=](Socket&& sock, write_buffer_getter&& bg, view::rvec remain, StateContext&& as, bool was_err) {
                    auto req = bg.req;
                    if (was_err) {
                        return;  // discard
                    }
                    as.log(log_level::debug, "reenter from async write", req->client.addr);
                    do_write(serv, std::move(req), remain, as, next);
                };
                as.log(log_level::debug, "start writing async", req->client.addr);
                auto sock = req->client.sock.clone();
                if (!as.write_async(sock, write_buffer_getter{std::move(req), buf}, fn)) {
                    sock.shutdown();  // discard
                }
            }

            void do_write(HTTPServ* serv, std::shared_ptr<Transport>&& req, view::rvec remain, StateContext& as, auto&& cb) {
                if (remain.size()) {
                    as.log(log_level::debug, "start writing", req->client.addr);
                }
                while (remain.size()) {
                    auto res = req->client.sock.write(remain);
                    if (!res) {
                        if (isSysBlock(res.error())) {
                            call_write_async(serv, std::move(req), remain, as, std::forward<decltype(cb)>(cb));
                            return;
                        }
                        as.log(log_level::err, &req->client.addr, res.error());
                        req->client.sock.shutdown();
                        return;
                    }
                    remain = *res;
                }
                cb(serv, std::move(req), std::move(as));
            }

            void handle_http2_streams(std::shared_ptr<Transport>&& req, StateContext as, std::shared_ptr<http2::FrameHandler>&& h) {
                slib::set<std::shared_ptr<http2::stream::Stream>> streams;
                auto err = h->add_data_and_read_frames(req->read_buf, [&](auto&& s) {
                    streams.emplace(s);
                });
                if (err) {
                    as.log(log_level::err, &req->client.addr, err);
                    h->send_goaway(int(err.code), err.debug);
                    return;
                }
                req->read_buf.clear();
                for (auto& s : streams) {
                    auto r = s->get_or_set_application_data<Requester>([&](std::shared_ptr<Requester>&& r, auto&& set) {
                        if (!r) {
                            r = std::allocate_shared<Requester>(glheap_allocator<Requester>());
                            set(r);
                            r->addr = req->client.addr;
                            r->http = http::HTTP(http2::HTTP2(s));
                        }
                        return r;
                    });
                    req->internal_->next(req->internal_->c, std::move(r), as);
                }
                req->flush(as, std::move(h->get_write_buffer().take_buffer()));
            }

            void do_read(HTTPServ* serv, std::shared_ptr<Transport> req, StateContext s);

            void may_init_version_specific(HTTPServ* serv, std::shared_ptr<Transport>& req, StateContext& as) {
                if (!req->version_specific) {
                    auto init_http1 = [&]() {
                        req->version = http::HTTPVersion::http1;
                        auto requester = std::allocate_shared<Requester>(glheap_allocator<Requester>());
                        requester->addr = req->client.addr;
                        requester->http = http::HTTP(http1::HTTP1());
                        req->version_specific = requester;
                    };
                    auto init_http2 = [&]() {
                        req->version = http::HTTPVersion::http2;
                        auto h2 = std::allocate_shared<http2::FrameHandler>(glheap_allocator<http2::FrameHandler>(), http2::HTTP2Role::server);
                        req->version_specific = h2;
                        auto err = h2->send_settings({.enable_push = false});
                        if (err) {
                            as.log(log_level::err, &req->client.addr, err);
                            req->may_shutdown_tls(as);
                            return;
                        }
                        // add data
                        req->flush(as, std::move(h2->get_write_buffer().take_buffer()));
                    };
                    if (req->tls) {
                        if (req->tls.in_handshake()) {
                            return;
                        }
                        as.log(log_level::debug, "tls handshake done", req->client.addr);
                        if (auto version = req->tls.get_tls_version()) {
                            as.log(log_level::debug, req->client.addr, "tls version ", *version);
                        }
                        auto alpn = req->tls.get_selected_alpn();
                        if (!alpn || alpn == "http/1.1") {
                            init_http1();
                        }
                        else if (alpn == "h2") {
                            init_http2();
                        }
                        else {
                            as.log(log_level::err, &req->client.addr, http::unknown_http_version);
                            req->may_shutdown_tls(as);
                        }
                    }
                    else {
                        if (serv->prefer_http2) {
                            init_http2();
                        }
                        else {
                            init_http1();
                        }
                    }
                }
            }

            void http_handler_impl(HTTPServ* serv, std::shared_ptr<Transport>&& req, StateContext as) {
                may_init_version_specific(serv, req, as);
                if (req->read_buf.size() == 0) {
                    if (req->write_buf.size()) {
                        as.log(log_level::debug, "write data before read", req->client.addr);
                        do_write(serv, std::move(req), req->write_buf, as, [](HTTPServ* s, std::shared_ptr<Transport>&& t, StateContext&& c) {
                            t->write_buf.clear();
                            do_read(s, std::move(t), std::move(c));
                        });
                    }
                    else {
                        as.log(log_level::debug, "close connection", req->client.addr);
                    }
                    return;
                }
                if (serv && serv->next) {
                    req->internal_ = serv;
                    if (auto h2 = req->http2_frame_handler()) {
                        handle_http2_streams(std::move(req), as, std::move(h2));
                    }
                    else if (auto h1 = req->http1_requester()) {
                        h1->http.http1()->add_input(req->read_buf);
                        req->read_buf.clear();
                        // call http handler
                        serv->next(serv->c, std::move(h1), as);
                        auto h1c = h1->http.http1();
                        req->flush(as, h1c->get_output());
                        h1c->adjust_input();
                        h1c->clear_output();
                    }
                    if (req) {  // not been moved
                        auto after = [](HTTPServ* s, std::shared_ptr<Transport>&& t, StateContext&& c) {
                            t->write_buf.clear();
                            if (auto h1 = t->http1_requester()) {
                                auto h = h1->http.http1();
                                if (h->read_ctx.on_no_body_semantics() && response_sent(*h1) && h->read_ctx.is_keep_alive()) {
                                    c.log(log_level::debug, "connection keep-alive via http1", t->client.addr);
                                    response_sent(*h1) = false;
                                    h->read_ctx.reset();
                                    h->write_ctx.reset();
                                    do_read(s, std::move(t), std::move(c));
                                }
                                else if (h->read_ctx.is_resumable() && !response_sent(*h1) && !h->read_ctx.on_no_body_semantics()) {
                                    c.log(log_level::debug, "wait for data", t->client.addr);
                                    do_read(s, std::move(t), std::move(c));
                                }
                                else {
                                    c.log(log_level::debug, "connection close", t->client.addr);
                                }
                            }
                            else if (auto h2 = t->http2_frame_handler()) {
                                if (h2->is_closed()) {
                                    c.log(log_level::debug, "connection close", t->client.addr);
                                }
                                else {
                                    c.log(log_level::debug, "connection keep-alive via http2", t->client.addr);
                                    do_read(s, std::move(t), std::move(c));
                                }
                            }
                        };
                        if (req->write_buf.size()) {
                            as.log(log_level::debug, "write data", req->client.addr);
                            do_write(serv, std::move(req), req->write_buf, as, after);
                        }
                        else {
                            after(serv, std::move(req), std::move(as));
                        }
                    }
                }
            }

            void do_read(HTTPServ* serv, std::shared_ptr<Transport> req, StateContext s) {
                s.log(log_level::debug, "start reading", req->client.addr);

                // bool done = false;
                socket_read_common(
                    std::move(req->client.sock),
                    [req, s](view::rvec r) { req->add_data(r, s); },
                    [&](Socket&& sock, auto&& add_data, auto&& then, auto&& error) {
                        call_read_async(serv, req, s);
                    },
                    [req, s](Socket&& sock) {
                        auto tmp = req;
                        http_handler_impl(req->internal_, std::move(tmp), s);
                    },
                    [req, s](error::Error err) {
                        req->client.sock.shutdown();
                        auto ss = s;
                        ss.log(log_level::warn, &req->client.addr, err);
                    });

                /*
                if (auto result = req->client.sock.read_until_block([&](view::wvec r) {
                        req->add_data(r, s);
                    });
                    !result) {
                    if (isSysBlock(result.error())) {
                        // no data available, so wait async
                        call_read_async(serv, req, s);
                        return;
                        // otherwise, start handling
                    }
                    else {
                        s.log(log_level::warn, &req->client.addr, result.error());
                        return;  // discard
                    }
                }
                http_handler_impl(serv, std::move(req), std::move(s));
                */
            }

            fnetserv_dll_internal(void) http_handler(void* v, Client&& cl, StateContext s) {
                s.log(log_level::perf, start_timing, cl.addr);
                auto serv = static_cast<HTTPServ*>(v);
                auto req = std::allocate_shared<Transport>(glheap_allocator<Transport>());
                req->internal_ = serv;
                req->client = std::move(cl);
                auto addr = req->client.sock.get_local_addr();
                if (!addr) {
                    s.log(log_level::err, &req->client.addr, addr.error());
                    return;
                }
                if (addr->port() == serv->secure_port && serv->tls_config) {
                    auto t = tls::create_tls_with_error(serv->tls_config);
                    if (!t) {
                        s.log(log_level::err, &req->client.addr, t.error());
                        req->client.sock.shutdown();
                        return;
                    }
                    req->tls = std::move(t.value());
                    // start tls handshake
                    auto ok = req->tls.accept();
                    if (!ok && !tls::isTLSBlock(ok.error())) {
                        s.log(log_level::err, &req->client.addr, ok.error());
                        req->client.sock.shutdown();
                        return;
                    }
                }
                do_read(serv, std::move(req), std::move(s));
            }

        }  // namespace server
    }  // namespace fnet
}  // namespace futils
