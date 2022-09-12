/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/server/servcpp.h>
#include <dnet/http.h>
#include <dnet/server/state.h>
#include <dnet/server/httpserv.h>
#include <wrap/cout.h>

namespace utils {
    namespace dnet {
        namespace server {
            void add_http(Requester& req, const char* data, size_t len) {
                req.http.add_input(data, len);
            }

            Socket& get_request_sock(Requester& req) {
                return req.client.sock;
            }
            void http_handler_impl(HTTPServ* serv, Requester&& req, StateContext as);

            void call_read_async(HTTPServ* serv, Requester& req, StateContext& as) {
                auto fn = [=](Requester&& req, StateContext&& as) {
                    http_handler_impl(serv, std::move(req), std::move(as));
                };
                if (!as.read_async(req.client.sock, fn, std::move(req), get_request_sock, add_http)) {
                    req.client.sock.shutdown();
                    as.log(debug, "failed to start read_async operation", req.client);
                }
            }

            void http_handler_impl(HTTPServ* serv, Requester&& req, StateContext as) {
                bool beg = false;
                if (!req.http.check_request(&beg)) {
                    if (!beg) {
                        req.client.sock.shutdown();
                        const char* data;
                        size_t len;
                        req.http.borrow_input(data, len);
                        as.log(debug, "invalid request received", req.client, data, len);
                        return;  // discard
                    }
                    call_read_async(serv, req, as);
                    return;
                }
                if (serv && serv->next) {
                    serv->next(serv->c, std::move(req), std::move(as));
                }
            }

            dnetserv_dll_internal(void) http_handler(void* v, Client&& cl, StateContext s) {
                s.log(perf, start_timing, cl);
                auto serv = static_cast<HTTPServ*>(v);
                char buf[1024];
                bool red = false;
                Requester req;
                req.client = std::move(cl);
                if (!req.client.sock.read_until_block(red, buf, sizeof(buf), [&] {
                        req.http.add_input(buf, req.client.sock.readsize());
                    })) {
                    s.log(debug, "connection closed at reading first data", req.client);
                    return;  // discard
                }
                if (!red) {
                    call_read_async(serv, req, s);
                    return;
                }
                http_handler_impl(serv, std::move(req), std::move(s));
            }
        }  // namespace server
    }      // namespace dnet
}  // namespace utils
