/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// httpserv - basic http serving
#pragma once
// external
#include "servh.h"
#include "client.h"
#include "../http.h"
// #include "../http2/http2.h"
#include "state.h"
#include "../tls/tls.h"

namespace futils {
    namespace fnet {
        namespace server {
            constexpr auto start_timing = "[hs]";
            struct Requester {
               private:
                void* internal_;

                friend void*& internal(Requester& req);

               public:
                Client client;
                tls::TLS tls;
                HTTP http;
                std::uintptr_t user_id;
                fnet::flex_storage read_buf;
                bool data_added = false;
                bool already_shutdown = false;

                view::wvec get_buffer() {
                    read_buf.resize(2048);
                    return read_buf;
                }

               private:
                void send_tls_data(StateContext& as) {
                    while (true) {
                        byte buf[1024];
                        auto data = tls.receive_tls_data(buf);
                        if (!data) {
                            if (!tls::isTLSBlock(data.error())) {
                                as.log(log_level::err, &client.addr, data.error());
                                client.sock.shutdown();
                            }
                            return;
                        }
                        client.sock.write(data.value());
                    }
                }

               public:
                void shutdown(StateContext& as) {
                    if (already_shutdown) {
                        return;
                    }
                    already_shutdown = true;
                    if (tls) {
                        tls.shutdown();
                        send_tls_data(as);
                    }
                    client.sock.shutdown();
                }

                void add_data(view::rvec d, StateContext as) {
                    if (d.size() == 0) {
                        return;
                    }
                    data_added = true;
                    if (tls) {
                        auto ok = tls.provide_tls_data(d);
                        if (!ok) {
                            if (!tls::isTLSBlock(ok.error())) {
                                as.log(log_level::err, &client.addr, ok.error());
                                shutdown(as);
                            }
                            else {
                                send_tls_data(as);
                            }
                            return;
                        }
                        while (true) {
                            byte buf[1024];
                            auto res = tls.read(buf);
                            if (!res) {
                                if (!tls::isTLSBlock(res.error())) {
                                    as.log(log_level::err, &client.addr, res.error());
                                    shutdown(as);
                                }
                                else {
                                    send_tls_data(as);
                                }
                                return;
                            }
                            http.add_input(res.value());
                        }
                    }
                    else {
                        http.add_input(d);
                    }
                }

                void flush(StateContext& as) {
                    if (tls) {
                        auto ok = tls.write(http.get_output());
                        if (!ok) {
                            if (!tls::isTLSBlock(ok.error())) {
                                as.log(log_level::err, &client.addr, ok.error());
                                shutdown(as);
                            }
                            else {
                                send_tls_data(as);
                            }
                            return;
                        }
                        send_tls_data(as);
                    }
                    else {
                        client.sock.write(http.get_output());
                    }
                }

                bool respond(auto&& status, auto&& header, view::rvec body = {}) {
                    number::Array<char, 40, true> buffer{};
                    number::to_string(buffer, body.size());
                    header.emplace("Content-Length", buffer.c_str());
                    return http.write_response(status, header, body);
                }

                bool respond_flush(StateContext& as, auto status, auto&& header, view::rvec body = {}) {
                    if (!respond(status, header, body)) {
                        return false;
                    }
                    flush(as);
                    return true;
                }
            };

            struct HTTPServ {
                tls::TLSConfig tls_config;
                void (*next)(void*, Requester req, StateContext s);
                void* c;
            };
            fnetserv_dll_export(void) http_handler(void* ctx, Client&& cl, StateContext s);

            enum class BodyState {
                complete,
                best_effort,
                error,
            };

            template <class T>
            using body_callback_fn = void (*)(Requester&& req, T* hdr, http::body::HTTPBodyInfo info, flex_storage&& body, BodyState s, StateContext c);

            fnetserv_dll_export(void) read_body(Requester&& req, void* hdr, http::body::HTTPBodyInfo info, StateContext s, body_callback_fn<void> cb);

            template <class T>
                requires(!std::is_void_v<T>)
            void read_body(Requester&& req, T* hdr, http::body::HTTPBodyInfo info, StateContext s, body_callback_fn<std::type_identity_t<T>> cb) {
                read_body(std::move(req), hdr, info, std::move(s), (body_callback_fn<void>)cb);
            }

            fnetserv_dll_export(void) handle_keep_alive(Requester&& cl, StateContext s);

            template <class TmpString>
            bool read_header_and_check_keep_alive(HTTP& http, auto&& method, auto&& path, auto&& header, bool& keep_alive, http::body::HTTPBodyInfo* body_info = nullptr) {
                number::Array<char, 20, true> version;
                keep_alive = false;
                bool close = false;
                if (!http.read_request<TmpString>(
                        method, path, [&](auto&& key, auto&& value) {
                            if (strutil::equal(key, "Connection", strutil::ignore_case())) {
                                if (strutil::contains(value, "close", strutil::ignore_case())) {
                                    close = true;
                                }
                                if (strutil::contains(value, "keep-alive", strutil::ignore_case())) {
                                    keep_alive = true;
                                }
                            }
                            return futils::http::header::apply_call_or_emplace(header, std::move(key), std::move(value));
                        },
                        body_info, false, version)) {
                    return false;  // no keep alive
                }
                if (close) {  // Connection: close
                    keep_alive = false;
                    return true;
                }
                if (keep_alive) {  // Connection: keep-alive
                    return true;
                }
                if (strutil::equal(version, "HTTP/1.0")) {  // HTTP/1.0 explicitly close
                    keep_alive = false;
                    return true;
                }
                keep_alive = true;  // HTTP/1.1 implicitly keep-alive
                return true;
            }

        }  // namespace server
    }      // namespace fnet
}  // namespace futils
