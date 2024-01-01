/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/server/servcpp.h>
#include <fnet/http.h>
#include <fnet/server/state.h>
#include <fnet/server/httpserv.h>
#include <wrap/cout.h>

namespace futils {
    namespace fnet {
        namespace server {

            void*& internal(Requester& req) {
                return req.internal_;
            }

            void http_handler_impl(HTTPServ* serv, Requester&& req, StateContext as);

            void call_read_async(HTTPServ* serv, Requester& req, StateContext& as) {
                if (req.already_shutdown) {
                    return;  // discard
                }
                auto fn = [=](Socket&& sock, Requester&& req, StateContext&& as, bool was_err) {
                    if (was_err) {
                        return;  // discard
                    }
                    if (req.already_shutdown) {
                        return;  // discard
                    }
                    as.log(log_level::debug, "reenter http handler from async read", req.client.addr);
                    http_handler_impl(serv, std::move(req), std::move(as));
                };
                req.data_added = false;
                if (!as.read_async(req.client.sock, fn, std::move(req))) {
                    req.client.sock.shutdown();  // discard
                }
            }

            void http_handler_impl(HTTPServ* serv, Requester&& req, StateContext as) {
                bool complete = false;
                if (!req.tls.in_handshake() && !req.http.strict_check_header(true, &complete)) {
                    if (req.tls && req.data_added) {
                        call_read_async(serv, req, as);
                        return;
                    }
                    req.client.sock.shutdown();
                    if (!req.data_added) {
                        as.log(log_level::info, "connection closed", req.client.addr);
                    }
                    else {
                        as.log(log_level::warn, "invalid request received", req.client.addr);
                    }
                    return;  // discard
                }
                else if (!complete) {
                    call_read_async(serv, req, as);
                    return;
                }
                if (serv && serv->next) {
                    internal(req) = serv;
                    serv->next(serv->c, std::move(req), std::move(as));
                }
            }

            void start_handling(HTTPServ* serv, Requester&& req, StateContext&& s) {
                char buf[1024];
                if (auto result = req.client.sock.read_until_block(buf, [&](view::wvec r) {
                        req.add_data(r, s);
                    });
                    !result) {
                    if (isSysBlock(result.error())) {
                        // no data available, so wait async
                        call_read_async(serv, req, s);
                        return;
                    }
                    s.log(log_level::warn, &req.client.addr, result.error());
                    return;  // discard
                }
                http_handler_impl(serv, std::move(req), std::move(s));
            }

            fnetserv_dll_internal(void) http_handler(void* v, Client&& cl, StateContext s) {
                s.log(log_level::perf, start_timing, cl.addr);
                auto serv = static_cast<HTTPServ*>(v);
                Requester req;
                req.client = std::move(cl);
                if (serv->tls_config) {
                    auto t = tls::create_tls_with_error(serv->tls_config);
                    if (!t) {
                        s.log(log_level::err, &req.client.addr, t.error());
                        req.client.sock.shutdown();
                        return;
                    }
                    req.tls = std::move(t.value());
                    // start tls handshake
                    auto ok = req.tls.accept();
                    if (!ok && !tls::isTLSBlock(ok.error())) {
                        s.log(log_level::err, &req.client.addr, ok.error());
                        req.client.sock.shutdown();
                        return;
                    }
                }
                start_handling(serv, std::move(req), std::move(s));
            }

            fnetserv_dll_export(void) handle_keep_alive(Requester&& req, StateContext s) {
                s.log(log_level::debug, "start keep-alive waiting", req.client.addr);
                req.http.clear_input();
                req.http.clear_output();
                auto serv = static_cast<HTTPServ*>(internal(req));
                start_handling(serv, std::move(req), std::move(s));
            }
        }  // namespace server
    }      // namespace fnet
}  // namespace futils
