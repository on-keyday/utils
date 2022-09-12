/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// http - http version 1
#pragma once
#include "dll/dllh.h"
#include "conn.h"
#include "http.h"
#include "dll/httpbuf.h"
#include <utility>

namespace utils {
    namespace dnet {
        enum class HTTPState {
            idle,
            sending,
            sending_done,
            receving_header,
            receving_body,
            receving_done,
            roundtrip_done,
            failed,
        };

        enum HTTPError {
            http_error_none,
            http_invalid_state,
            http_no_connection_exists,

            http_send_request,
            http_send_request_tls,
            http_recv_response,
            http_recv_response_tls,
            http_tls_io,

            http_render_request,
            http_render_response,
            http_invalid_response,
            http_recv_body,

            http_error_max,
        };

        struct dnet_class_export HTTPRoundTrip {
           private:
            HTTPState state = HTTPState::idle;
            HTTPError err = http_error_none;
            HTTP http;
            struct {
                bool header_written = false;
                bool body_written = false;
                bool started = false;
                HTTPBodyInfo info{};
                size_t body_from = 0;

            } http_flag;
            bool server_mode = false;

            bool (*do_roundtrip)(HTTPRoundTrip* h, bool start);
            static bool socket_sending(HTTPRoundTrip* h, bool start);
            static bool tls_sending(HTTPRoundTrip* h, bool start);
            static bool common_header_recv(HTTPRoundTrip* h);
            static bool common_body_recv(HTTPRoundTrip* h);
            static bool socket_data_recv(HTTPRoundTrip* h, bool start);
            static bool tls_data_recv(HTTPRoundTrip* h, bool start);
            constexpr bool is_connected() const {
                return conn.state == ConnState::tls_connected ||
                       conn.state == ConnState::connected;
            }

            constexpr bool set_sender() {
                if (!is_connected()) {
                    return false;
                }
                if (conn.state == ConnState::tls_connected) {
                    do_roundtrip = tls_sending;
                }
                else {
                    do_roundtrip = socket_sending;
                }
                return true;
            }

            constexpr bool set_receiver() {
                if (!is_connected()) {
                    return false;
                }
                if (conn.state == ConnState::tls_connected) {
                    do_roundtrip = tls_data_recv;
                }
                else {
                    do_roundtrip = socket_data_recv;
                }
                return true;
            }

           public:
            Conn conn;

            bool start_tls(const char* host) {
                return conn.start_tls(host, "\x08http/1.1", 9);
            }

            void reset();
            bool request(auto&& method, auto&& path, auto&& header) {
                if (server_mode) {
                    return false;
                }
                if (http_flag.started) {
                    err = http_invalid_state;
                    return false;
                }
                http.clear_output();
                http_flag = {};
                if (!http.write_request(method, path, header)) {
                    err = http_render_request;
                    return false;
                }

                http_flag.header_written = true;
                return true;
            }

            bool response(auto&& status, auto&& header) {
                if (!server_mode) {
                    return false;
                }
                if (http_flag.started) {
                    err = http_invalid_state;
                    return false;
                }
                http.clear_output();
                http_flag = {};
                if (!http.write_response(status, header)) {
                    err = http_render_response;
                    return false;
                }
                http_flag.header_written = true;
                return true;
            }

            bool body(auto&& data, size_t len, bool end = true) {
                if (http_flag.started || !http_flag.header_written || http_flag.body_written) {
                    return false;
                }
                http.write_data(data, len);
                http_flag.body_written = end;
            }

            bool chunked_body(auto&& data, size_t len) {
                if (http_flag.started || !http_flag.header_written || http_flag.body_written) {
                    return false;
                }
                http.write_chunked_data(data, len);
                if (len == 0) {
                    http_flag.body_written = true;
                }
            }

            bool start_sending();
            bool wait_for_sending_done();
            bool wait_for_receiving();
            constexpr bool failed() const {
                return state == HTTPState::failed;
            }
        };

    }  // namespace dnet
}  // namespace utils
