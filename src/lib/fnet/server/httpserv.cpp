/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/server/servcpp.h>
#include <fnet/http.h>
#include <fnet/server/state.h>
#include <fnet/server/httpserv.h>
#include <wrap/cout.h>

namespace utils {
    namespace fnet {
        namespace server {
            void add_http(Requester& req, const char* data, size_t len) {
                req.http.add_input(data, len);
            }

            Socket& get_request_sock(Requester& req) {
                return req.client.sock;
            }
            void http_handler_impl(HTTPServ* serv, Requester&& req, StateContext as, bool was_err);

            void call_read_async(HTTPServ* serv, Requester& req, StateContext& as) {
                auto fn = [=](Socket&& sock, Requester&& req, StateContext&& as, bool was_err) {
                    req.client.sock = std::move(sock);  // restore
                    as.log(log_level::debug, "reenter http handler from async read", req.client.addr);
                    http_handler_impl(serv, std::move(req), std::move(as), was_err);
                };
                if (!as.read_async(req.client.sock, fn, std::move(req))) {
                    req.client.sock.shutdown();  // discard
                }
            }

            void http_handler_impl(HTTPServ* serv, Requester&& req, StateContext as, bool was_err) {
                if (was_err) {
                    return;  // discard
                }
                bool complete = false;
                if (!req.http.strict_check_header(true, &complete)) {
                    req.client.sock.shutdown();
                    const char* data;
                    size_t len;
                    req.http.borrow_input(data, len);
                    if (len == 0) {
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
                    req.internal_ = serv;
                    serv->next(serv->c, std::move(req), std::move(as));
                }
            }

            void start_handling(HTTPServ* serv, Requester&& req, StateContext&& s) {
                char buf[1024];
                if (auto result = req.client.sock.read_until_block(buf, [&](view::wvec r) {
                        req.http.add_input(r, r.size());
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
                http_handler_impl(serv, std::move(req), std::move(s), false);
            }

            fnetserv_dll_internal(void) http_handler(void* v, Client&& cl, StateContext s) {
                s.log(log_level::perf, start_timing, cl.addr);
                auto serv = static_cast<HTTPServ*>(v);
                Requester req;
                req.client = std::move(cl);
                start_handling(serv, std::move(req), std::move(s));
            }

            fnetserv_dll_export(void) handle_keep_alive(Requester&& req, StateContext s) {
                s.log(log_level::debug, "start keep-alive waiting", req.client.addr);
                req.http.clear_input();
                req.http.clear_output();
                auto serv = static_cast<HTTPServ*>(req.internal_);
                start_handling(serv, std::move(req), std::move(s));
            }
        }  // namespace server
    }      // namespace fnet
}  // namespace utils
