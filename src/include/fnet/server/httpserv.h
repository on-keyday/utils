/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// httpserv - basic http serving
#pragma once
// external
#include "servh.h"
#include "client.h"
#include "../http.h"
#include "../http2/http2.h"
#include "state.h"
#include "../tls/tls.h"

namespace utils {
    namespace fnet {
        namespace server {
            constexpr auto start_timing = "[hs]";
            struct Requester {
                void* internal_;
                Client client;
                HTTP http;
                std::uintptr_t user_id;
                fnet::flex_storage read_buf;

                view::wvec get_buffer() {
                    read_buf.resize(2000);
                    return read_buf;
                }

                void add_data(view::rvec d) {
                    http.add_input(d, d.size());
                }

                template <class Body = const char*>
                bool respond(int status, auto&& header, Body&& body = "", size_t len = 0) {
                    number::Array<char, 40, true> lenstr{};
                    number::to_string(lenstr, len);
                    header.emplace("Content-Length", lenstr.c_str());
                    return http.write_response(status, header, body, len);
                }

                void flush() {
                    const char* data;
                    size_t len;
                    http.borrow_output(data, len);
                    client.sock.write(view::rvec(data, len));
                }

                template <class Body = const char*>
                bool respond_flush(int status, auto&& header, Body&& body = "", size_t len = 0) {
                    if (!respond(status, header, body, len)) {
                        return false;
                    }
                    flush();
                    return true;
                }
            };

            struct HTTPServ {
                void (*next)(void*, Requester req, StateContext s);
                void* c;
            };
            fnetserv_dll_export(void) http_handler(void* ctx, Client&& cl, StateContext s);
            fnetserv_dll_export(void) handle_keep_alive(Requester&& cl, StateContext s);

            template <class TmpString>
            bool has_keep_alive(HTTP& http, auto&& header) {
                number::Array<char, 20, true> version;
                bool keep_alive = false;
                bool close = false;
                if (!http.read_request<TmpString>(helper::nop, helper::nop, header, nullptr, true, version, [&](auto&& key, auto&& value) {
                        if (helper::equal(key, "Connection", helper::ignore_case())) {
                            if (helper::contains(value, "close")) {
                                close = true;
                            }
                            if (helper::contains(value, "keep-alive") ||
                                helper::contains(value, "Keep-Alive")) {
                                keep_alive = true;
                            }
                        }
                    })) {
                    return false;  // no keep alive
                }
                if (close) {
                    return false;
                }
                if (keep_alive) {
                    return true;
                }
                if (helper::equal(version, "HTTP/1.0")) {
                    return false;  // connection close
                }
                return true;  // HTTP/1.1 implicitly keep-alive
            }

        }  // namespace server
    }      // namespace fnet
}  // namespace utils
