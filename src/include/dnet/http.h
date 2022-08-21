/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// http - http version 1
#pragma once
#include "dll/dllh.h"
#include "addrinfo.h"
#include "socket.h"
#include "tls.h"
#include "../net/http/http_headers.h"
#include <utility>

namespace utils {
    namespace dnet {
        enum class HTTPState {
            idle,
            wait_resolve,
            resolved,
            start_connect,
            connected,
            tls_start,
            tls_connected,
            sending_request,
            request_done,
            receving_response,
            receving_body,
            response_done,
            failed,
        };

        enum HTTPError {
            http_error_none,
            http_invalid_state,
            http_invalid_argument,

            http_socket_layer_error,
            http_start_resolve,
            http_fail_resolvement,
            http_start_connect,
            http_make_connection,
            http_send_request,
            http_recv_response,

            http_tls_layer_error,
            http_fail_create_tls,
            http_fail_make_ssl,
            http_fail_set_alpn,
            http_fail_set_host,
            http_start_tls,
            http_make_tls,
            http_make_tls_connection,
            http_send_request_tls,
            http_recv_response_tls,
            http_tls_io,

            http_http_layer_error,
            http_set_header,
            http_render_request,
            http_invalid_response,
            http_recv_body,

            http_error_max,
        };

        struct Conn {
            dnet::AddrInfo addr;
            dnet::Socket sock;
            dnet::TLS tls;
        };

        struct class_export HTTPBuf {
           private:
            char* text = nullptr;
            size_t size = 0;
            size_t cap = 0;
            friend struct HTTP;
            friend struct HTTPServer;
            friend struct HTTPPushBack;
            ~HTTPBuf();
            HTTPBuf(size_t c);

           public:
            constexpr HTTPBuf() = default;
            constexpr HTTPBuf(HTTPBuf&& buf)
                : text(std::exchange(buf.text, nullptr)),
                  size(std::exchange(buf.size, 0)),
                  cap(std::exchange(buf.cap, 0)) {}
            constexpr HTTPBuf& operator=(HTTPBuf&& buf) {
                if (this == &buf) {
                    return *this;
                }
                this->~HTTPBuf();
                text = std::exchange(buf.text, nullptr);
                size = std::exchange(buf.size, 0);
                cap = std::exchange(buf.cap, 0);
                return *this;
            }
        };

        struct class_export HTTPPushBack {
           private:
            HTTPBuf* buf = nullptr;
            friend struct HTTP;
            friend struct HTTPServer;

           public:
            constexpr HTTPPushBack(HTTPBuf* b)
                : buf(b) {}
            void push_back(unsigned char c);
        };

        struct class_export HTTP {
           private:
            HTTPState state = HTTPState::idle;
            dnet::WaitAddrInfo wait;
            Conn conn;
            struct {
                HTTPBuf buf;
                TLSIOState state = TLSIOState::idle;
                bool write_done = false;
            } tls;
            struct {
                HTTPBuf request_header;
                HTTPBuf response_header;
                HTTPBuf response_body;
                HTTPBuf tempbuf;  // temporary sending/receiveing buffer
                net::h1header::StatusCode status_code;
                net::h1body::BodyType btype = net::h1body::BodyType::no_info;
                size_t expect = 0;
            } http;
            HTTPError err = http_error_none;
            bool (*do_request)(HTTP* h, bool start);
            static bool socket_request(HTTP* h, bool start);
            static bool tls_request(HTTP* h, bool start);
            static bool common_response_recv(HTTP* h);
            static bool common_body_recv(HTTP* h);
            static bool socket_response_recv(HTTP* h, bool start);
            static bool tls_response_recv(HTTP* h, bool start);

           public:
            void reset();
            bool start_resolve(const char* host, const char* port);
            bool wait_for_resolve(std::uint32_t msec);
            bool start_connect();
            bool wait_for_connect(std::uint32_t sec, std::uint32_t usec);
            bool set_cert(const char* cacert);
            bool start_tls(const char* host);
            bool wait_for_tls();
            template <class Header>
            bool set_header(Header&& h) {
                using namespace net::h1header;
                http.request_header = {100};  // reset
                http.request_header.size = 0;
                HTTPPushBack pb{&http.request_header};
                if (!render_header_common(pb, h, default_validator())) {
                    err = http_set_header;
                    return false;
                }
                return true;
            }
            bool request(const char* method, const char* path,
                         const void* data = nullptr, size_t len = 0);
            bool wait_for_request_done();
            bool wait_for_response();
            constexpr bool failed() const {
                return state == HTTPState::failed;
            }
        };

        dll_export(bool) do_tls_io_loop(Socket& sock, TLS& tls, TLSIOState& state, char* text, size_t& size, size_t cap);
    }  // namespace dnet
}  // namespace utils
