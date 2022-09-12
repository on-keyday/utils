/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// httpserv - basic http serving
#pragma once
// external
#include "servh.h"
#include "client.h"
#include "../http.h"
#include "state.h"

namespace utils {
    namespace dnet {
        namespace server {
            constexpr auto start_timing = "[hs]";
            struct Requester {
                Client client;
                HTTP http;
                std::uintptr_t user_id;

                template <class Body = const char*>
                bool respond(int status, auto&& header, Body&& body = "", size_t len = 0) {
                    number::Array<40, char, true> lenstr{};
                    number::to_string(lenstr, len);
                    header.emplace("Content-Length", lenstr.c_str());
                    return http.write_response(status, header, body, len);
                }

                void flush() {
                    const char* data;
                    size_t len;
                    http.borrow_output(data, len);
                    client.sock.write(data, len);
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
            dnetserv_dll_export(void) http_handler(void* ctx, Client&& cl, StateContext s);
        }  // namespace server
    }      // namespace dnet
}  // namespace utils
