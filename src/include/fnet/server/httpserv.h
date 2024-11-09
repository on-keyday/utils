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
#include "../http/http.h"
#include "state.h"
#include "../tls/tls.h"

namespace futils {
    namespace fnet {
        namespace server {
            constexpr auto start_timing = "[hs]";

            struct HTTPServ;

            struct Requester;

            // handles tcp based transport
            // on quic based, another transport should be used
            struct Transport {
                byte async_read_buf[2048]{};
                HTTPServ* internal_ = nullptr;
                fnet::flex_storage read_buf;
                fnet::flex_storage write_buf;
                Client client;
                tls::TLS tls;
                http::HTTPVersion version = http::HTTPVersion::unknown;
                std::shared_ptr<void> version_specific;

                std::shared_ptr<Requester> http1_requester() {
                    if (version != http::HTTPVersion::http1) {
                        return nullptr;
                    }
                    return std::static_pointer_cast<Requester>(version_specific);
                }

                std::shared_ptr<http2::FrameHandler> http2_frame_handler() {
                    if (version != http::HTTPVersion::http2) {
                        return nullptr;
                    }
                    return std::static_pointer_cast<http2::FrameHandler>(version_specific);
                }

                view::wvec get_buffer() {
                    return async_read_buf;
                }

                void add_to_buffer(view::rvec d) {
                    read_buf.append(d);
                }

                void write_tls_data(StateContext& as) {
                    while (true) {
                        byte buf[1024];
                        auto data = tls.receive_tls_data(buf);
                        if (!data) {
                            if (!tls::isTLSBlock(data.error())) {
                                as.log(log_level::err, &client.addr, data.error());
                                // cannot shutdown, because this is fatal
                            }
                            return;
                        }
                        write_buf.append(*data);
                    }
                }

                void add_data(view::rvec d, StateContext as) {
                    if (d.size() == 0) {
                        return;
                    }
                    if (tls) {
                        auto ok = tls.provide_tls_data(d);
                        if (!ok) {
                            if (!tls::isTLSBlock(ok.error())) {
                                as.log(log_level::err, &client.addr, ok.error());
                                may_shutdown_tls(as);
                            }
                            else {
                                write_tls_data(as);
                            }
                            return;
                        }
                        while (true) {
                            byte buf[1024];
                            auto res = tls.read(buf);
                            if (!res) {
                                if (!tls::isTLSBlock(res.error())) {
                                    as.log(log_level::err, &client.addr, res.error());
                                    may_shutdown_tls(as);
                                }
                                else {
                                    write_tls_data(as);
                                }
                                return;
                            }
                            add_to_buffer(res.value());
                        }
                    }
                    else {
                        add_to_buffer(d);
                    }
                }

                void may_shutdown_tls(StateContext& as) {
                    if (tls) {
                        tls.shutdown();
                        write_tls_data(as);
                    }
                }

                void flush(StateContext& as, view::rvec d) {
                    if (d.size() == 0) {
                        return;
                    }
                    if (tls) {
                        auto ok = tls.write(d);
                        if (!ok) {
                            if (!tls::isTLSBlock(ok.error())) {
                                as.log(log_level::err, &client.addr, ok.error());
                            }
                            else {
                                write_tls_data(as);
                            }
                            return;
                        }
                        write_tls_data(as);
                    }
                    else {
                        write_buf.append(d);
                    }
                }
            };

            struct Requester {
               private:
                friend bool& response_sent(Requester& req);
                std::shared_ptr<void> user_data;
                bool response_sent = false;

               public:
                http::HTTP http;
                fnet::NetAddrPort addr;

                template <class T>
                std::shared_ptr<T> get_or_set_user_data(auto&& create) {
                    auto setter = [&](auto&& to_set) {
                        user_data = std::forward<decltype(to_set)>(to_set);
                    };
                    return create(std::static_pointer_cast<T>(user_data), setter);
                }

               public:
                error::Error respond(auto&& status, auto&& header, view::rvec body = {}) {
                    if (response_sent) {
                        return error::Error("http response already sent", error::Category::app);
                    }
                    number::Array<char, 40, true> buffer{};
                    number::to_string(buffer, body.size());
                    header.emplace("Content-Length", buffer.c_str());
                    // handle keep-alive or close
                    if (auto h1 = http.http1()) {
                        if (h1->read_ctx.on_no_body_semantics() && h1->read_ctx.is_keep_alive()) {
                            header.emplace("Connection", "keep-alive");
                        }
                        else {
                            header.emplace("Connection", "close");
                        }
                    }
                    if (auto err = http.write_response(status, header, body)) {
                        return err;
                    }
                    response_sent = true;
                    return {};
                }
            };

            struct HTTPServ {
                tls::TLSConfig tls_config;
                // http2 is used even if cleartext
                bool prefer_http2 = false;
                std::uint32_t normal_port = 0;
                std::uint32_t secure_port = 0;
                void (*next)(void*, std::shared_ptr<Requester>&& req, StateContext s);
                void* c;
            };
            fnetserv_dll_export(void) http_handler(void* ctx, Client&& cl, StateContext s);

            fnetserv_dll_export(void) http3_server_setup(quic::server::MultiplexerConfig<quic::use::smartptr::DefaultTypeConfig>& c);
        }  // namespace server
    }  // namespace fnet
}  // namespace futils
