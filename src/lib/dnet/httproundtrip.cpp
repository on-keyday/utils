/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/sockdll.h>
#include <dnet/httproundtrip.h>
#include <dnet/dll/httpbufproxy.h>
#include <dnet/dll/glheap.h>
#include <cstring>

namespace utils {
    namespace dnet {

        void HTTPRoundTrip::reset() {
            TLS tls_p;
            if (this->conn.tls) {
                tls_p = create_tls_from(this->conn.tls);
            }
            this->conn = {};
            this->conn.tls = std::move(tls_p);
            this->do_roundtrip = nullptr;
            this->http = {};
            this->err = http_error_none;
            this->state = HTTPState::idle;
        }

        constexpr size_t strlen(auto v) {
            size_t i = 0;
            while (v[i]) {
                i++;
            }
            return i;
        }

        bool HTTPRoundTrip::socket_sending(HTTPRoundTrip* h, bool start) {
            if (h->state != HTTPState::sending) {
                h->err = http_invalid_state;
                return false;
            }
            const char* ptr;
            size_t size;
            h->http.borrow_output(ptr, size);
            auto res = h->conn.sock.write(ptr, size);
            if (!res) {
                if (!h->conn.sock.block()) {
                    h->state = HTTPState::failed;
                    h->err = http_send_request;
                    return false;
                }
                return start;
            }
            else {
                if (h->server_mode) {
                    h->state = HTTPState::roundtrip_done;
                }
                else {
                    h->state = HTTPState::sending_done;
                }
                h->http.get_output(helper::nop);
            }
            return start || res;
        }

        bool HTTPRoundTrip::tls_sending(HTTPRoundTrip* h, bool start) {
            if (h->state != HTTPState::sending) {
                h->err = http_invalid_state;
                return false;
            }
            bool write_done = false;
            const char* ptr;
            size_t size;
            h->http.borrow_output(ptr, size);
            auto res = h->conn.tls.write(ptr, size);
            if (!res) {
                if (!h->conn.tls.block()) {
                    h->err = http_send_request_tls;
                    h->state = HTTPState::failed;
                    return false;
                }
            }
            else {
                if (h->server_mode) {
                    h->state = HTTPState::roundtrip_done;
                }
                else {
                    h->state = HTTPState::sending_done;
                }
                write_done = true;
                h->http.get_output(helper::nop);
            }
            if (!h->conn.tls_io()) {
                h->err = http_send_request_tls;
                h->state = HTTPState::failed;
                return false;
            }
            return start || write_done;
        }

        bool HTTPRoundTrip::start_sending() {
            if (server_mode) {
                if (state != HTTPState::receving_done) {
                    err = http_invalid_state;
                    return false;
                }
            }
            else {
                if (state != HTTPState::idle) {
                    err = http_invalid_state;
                    return false;
                }
            }
            if (!http_flag.header_written) {
                return false;
            }
            if (!set_sender()) {
                err = http_no_connection_exists;
                return false;
            }
            http_flag.started = true;
            state = HTTPState::sending;
            return do_roundtrip(this, true);
        }

        bool HTTPRoundTrip::wait_for_sending_done() {
            if (state != HTTPState::sending) {
                if (state == HTTPState::sending_done) {
                    return true;
                }
                err = http_invalid_state;
                return false;
            }
            return do_roundtrip(this, false);
        }

        using TmpString = number::Array<90, char, true>;

        struct TmpHeader {
            void emplace(TmpString&& key, TmpString&& value) {
                auto debug_ = key[0] == value[0];
            }
        };

        bool HTTPRoundTrip::common_body_recv(HTTPRoundTrip* h) {
            bool invalid = false;
            auto res = h->http.read_body(helper::nop, h->http_flag.info, 1, h->http_flag.body_from, &invalid);
            if (invalid) {
                h->err = http_recv_body;
                h->state = HTTPState::failed;
                return false;
            }
            if (!res) {
                return false;
            }
            if (h->server_mode) {
                h->state = HTTPState::receving_done;
            }
            else {
                h->state = HTTPState::roundtrip_done;
            }
            return true;
        }

        bool HTTPRoundTrip::common_header_recv(HTTPRoundTrip* h) {
            if (h->state == HTTPState::receving_body) {
                return common_body_recv(h);
            }
            bool begok = false, res = false;
            if (h->server_mode) {
                res = h->http.check_request(&begok);
            }
            else {
                res = h->http.check_response(&begok);
            }
            if (!begok) {
                h->err = http_invalid_response;
                h->state = HTTPState::failed;
                return false;
            }
            if (res) {
                size_t expect = 0;
                TmpHeader tmp{};
                if (h->server_mode) {
                    h->http_flag.body_from = h->http.read_request<TmpString>(helper::nop, helper::nop, tmp, &h->http_flag.info, true);
                }
                else {
                    h->http_flag.body_from = h->http.read_response<TmpString>(helper::nop, tmp, &h->http_flag.info, true);
                }
                if (h->http_flag.body_from == 0) {
                    h->err = http_invalid_response;
                    h->state = HTTPState::failed;
                    return false;
                }
                return common_body_recv(h);
            }
            return false;
        }

        bool HTTPRoundTrip::socket_data_recv(HTTPRoundTrip* h, bool start) {
            char buf[1024];
            bool red = false;
            while (h->conn.sock.read(buf, sizeof(buf))) {
                h->http.add_input(buf, h->conn.sock.readsize());
                red = true;
            }
            if (!h->conn.sock.block()) {
                h->state = HTTPState::failed;
                h->err = http_recv_response;
                return false;
            }
            if (!red) {
                return false;
            }
            return common_header_recv(h);
        }

        bool HTTPRoundTrip::tls_data_recv(HTTPRoundTrip* h, bool start) {
            char buf[1024];
            bool red = false;
            while (h->conn.tls.read(buf, sizeof(buf))) {
                h->http.add_input(buf, h->conn.tls.readsize());
                red = true;
            }
            if (!h->conn.tls.block() && !h->conn.tls.closed()) {
                h->err = http_recv_response_tls;
                h->state = HTTPState::failed;
                return false;
            }
            if (!h->conn.tls.closed()) {
                if (!h->conn.tls_io()) {
                    h->err = http_tls_io;
                    h->state = HTTPState::failed;
                    return false;
                }
            }
            if (!red) {
                return false;
            }
            return common_header_recv(h);
        }

        bool HTTPRoundTrip::wait_for_receiving() {
            bool enter = false;
            if (server_mode) {
                enter = state == HTTPState::idle;
            }
            else {
                enter = state == HTTPState::sending_done;
            }
            if (!enter &&
                state != HTTPState::receving_header &&
                state != HTTPState::receving_body) {
                if (state == HTTPState::roundtrip_done) {
                    return true;
                }
                err = http_invalid_state;
                return false;
            }
            if (!set_receiver()) {
                err = http_no_connection_exists;
                return false;
            }
            if (enter) {
                state = HTTPState::receving_header;
            }
            return do_roundtrip(this, false);
        }

    }  // namespace dnet
}  // namespace utils
