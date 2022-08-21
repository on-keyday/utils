/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/sockdll.h>
#include <dnet/http.h>
#include <dnet/dll/glheap.h>
#include <cstring>

namespace utils {
    namespace dnet {

        void HTTP::reset() {
            this->wait = {};
            TLS tls_p;
            if (this->conn.tls) {
                tls_p = create_tls_from(this->conn.tls);
            }
            this->conn = {};
            this->conn.tls = std::move(tls_p);
            this->do_request = nullptr;
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

        bool HTTP::start_resolve(const char* host, const char* port) {
            if (state != HTTPState::idle) {
                err = http_invalid_state;
                return false;
            }
            if (!host || !port) {
                err = http_invalid_argument;
                return false;
            }
            SockAddr hint{};
            hint.hostname = host;
            hint.namelen = strlen(host);
            hint.type = SOCK_STREAM;
            wait = resolve_address(hint, port);
            if (wait.failed()) {
                err = http_start_resolve;
                return false;
            }
            state = HTTPState::wait_resolve;
            if (wait.wait(conn.addr, 0)) {  // try to resolve immediately
                state = HTTPState::resolved;
                return true;
            }
            if (wait.failed()) {
                state = HTTPState::failed;
                err = http_fail_resolvement;
                return false;
            }
            return true;
        }

        bool HTTP::wait_for_resolve(std::uint32_t time) {
            if (state != HTTPState::wait_resolve) {
                if (state == HTTPState::resolved) {
                    return true;
                }
                err = http_invalid_state;
                return false;
            }
            if (wait.wait(conn.addr, time)) {
                state = HTTPState::resolved;
                return true;
            }
            if (wait.failed()) {
                state = HTTPState::failed;
                err = http_fail_resolvement;
            }
            return false;
        }

        bool HTTP::start_connect() {
            if (state != HTTPState::resolved) {
                err = http_invalid_state;
                return false;
            }
            while (conn.addr.next()) {
                SockAddr addr;
                conn.addr.sockaddr(addr);
                auto tmp = make_socket(addr.af, addr.type, addr.proto);
                if (!tmp) {
                    continue;
                }
                if (!tmp.connect(addr.addr, addr.addrlen)) {
                    if (tmp.block()) {
                        state = HTTPState::start_connect;
                        conn.sock = std::move(tmp);
                        return true;
                    }
                    continue;
                }
                state = HTTPState::connected;
                conn.sock = std::move(tmp);
                return true;
            }
            state = HTTPState::failed;
            err = http_start_connect;
            return true;
        }

        bool HTTP::wait_for_connect(std::uint32_t sec, std::uint32_t usec) {
            if (state != HTTPState::start_connect) {
                if (state == HTTPState::connected) {
                    return true;
                }
                err = http_invalid_state;
                return true;
            }
            if (!conn.sock.wait_writable(sec, usec)) {
                if (!conn.sock.block()) {
                    state = HTTPState::failed;
                    err = http_make_connection;
                    return false;
                }
                return false;
            }
            state = HTTPState::connected;
            return true;
        }

        bool HTTP::set_cert(const char* cacert) {
            if (conn.tls.has_ssl()) {
                return false;
            }
            if (!conn.tls) {
                conn.tls = create_tls();
                if (!conn.tls) {
                    return false;
                }
            }
            return conn.tls.set_cacert_file(cacert);
        }

        bool HTTP::start_tls(const char* host) {
            if (state != HTTPState::connected) {
                err = http_invalid_state;
                return false;
            }
            if (!conn.tls) {
                conn.tls = create_tls();
                if (!conn.tls) {
                    err = http_fail_create_tls;
                    state = HTTPState::failed;
                    return false;
                }
            }
            auto rep = [&](auto h) {
                state = HTTPState::failed;
                err = h;
                return false;
            };
            if (!conn.tls.has_ssl() && !conn.tls.make_ssl()) {
                return rep(http_fail_make_ssl);
            }
            if (!conn.tls.set_alpn("\x08http/1.1", 9)) {
                return rep(http_fail_set_alpn);
            }
            if (!conn.tls.set_hostname(host)) {
                return rep(http_fail_set_host);
            }
            if (!conn.tls.connect()) {
                if (conn.tls.block()) {
                    state = HTTPState::tls_start;
                    return true;
                }
                return rep(http_start_tls);
            }
            state = HTTPState::tls_connected;
            return true;
        }

        HTTPBuf::~HTTPBuf() {
            free_cvec(text);
            text = nullptr;
        }

        HTTPBuf::HTTPBuf(size_t c) {
            text = get_cvec(c);
            if (text) {
                cap = c;
            }
        }

        void HTTPPushBack::push_back(unsigned char c) {
            if (!buf || !buf->text) {
                return;
            }
            if (buf->size + 1 >= buf->cap) {
                if (!resize_cvec(buf->text, buf->cap * 2)) {
                    return;
                }
                buf->cap <<= 1;
            }
            buf->text[buf->size] = c;
            buf->size++;
        }

        dll_internal(bool) do_tls_io_loop(Socket& sock, TLS& tls, TLSIOState& state, char* text, size_t& size, size_t cap) {
            auto do_write = [&] {
                if (!sock.write(text, size)) {
                    if (sock.block()) {
                        state = TLSIOState::to_provider;
                        return false;
                    }
                    state = TLSIOState::fatal;
                    return false;
                }
                return true;
            };
            auto do_provide = [&] {
                if (!tls.provide_tls_data(text, size)) {
                    if (tls.block()) {
                        state = TLSIOState::to_ssl;
                    }
                    else {
                        state = TLSIOState::fatal;
                    }
                    return false;
                }
                return true;
            };
            // state machine           <-                            <-
            // from_ssl (-> to_provider \) -> from_provider (-> to_ssl) |
            int count = 0;
            while (true) {
                if (state == TLSIOState::from_ssl) {
                    bool red = false;
                    while (tls.receive_tls_data(text, cap)) {
                        size = tls.readsize();
                        if (!do_write()) {
                            return state != TLSIOState::fatal;
                        }
                        red = true;
                    }
                    if (!red) {
                        count++;
                    }
                    state = TLSIOState::from_provider;
                }
                if (state == TLSIOState::to_provider) {
                    if (!do_write()) {
                        return state != TLSIOState::fatal;
                    }
                    state = TLSIOState::from_ssl;
                }
                if (state == TLSIOState::from_provider) {
                    bool red = false;
                    while (sock.read(text, cap)) {
                        if (sock.readsize() == 0) {
                            return true;
                        }
                        size = sock.readsize();
                        if (!do_provide()) {
                            return state != TLSIOState::fatal;
                        }
                        red = true;
                    }
                    if (!red) {
                        count++;
                    }
                    if (!sock.block()) {
                        state = TLSIOState::fatal;
                        return false;
                    }
                    state = TLSIOState::from_ssl;
                }
                if (state == TLSIOState::to_ssl) {
                    if (!do_provide()) {
                        return state != TLSIOState::fatal;
                    }
                    state = TLSIOState::from_provider;
                }
                if (count > 10) {
                    return true;
                }
            }
        }

        bool HTTP::wait_for_tls() {
            if (state != HTTPState::tls_start) {
                if (state == HTTPState::tls_connected) {
                    return true;
                }
                err = http_invalid_state;
                return false;
            }
            if (!tls.buf.text) {
                tls.buf = {1024 * 4};
            }
            if (tls.state == TLSIOState::idle) {
                tls.state = TLSIOState::from_ssl;
            }
            if (!do_tls_io_loop(conn.sock, conn.tls, tls.state, tls.buf.text, tls.buf.size, tls.buf.cap)) {
                state = HTTPState::failed;
                err = http_make_tls;
                return false;
            }
            if (conn.tls.connect()) {
                state = HTTPState::tls_connected;
                return true;
            }
            if (!conn.tls.block()) {
                state = HTTPState::failed;
                err = http_make_tls_connection;
            }
            return false;
        }

        bool HTTP::socket_request(HTTP* h, bool start) {
            if (h->state != HTTPState::sending_request) {
                h->err = http_invalid_state;
                return false;
            }
            auto res = h->conn.sock.write(h->http.tempbuf.text, h->http.tempbuf.size);
            if (!res) {
                if (!h->conn.sock.block()) {
                    h->state = HTTPState::failed;
                    h->err = http_send_request;
                    return false;
                }
                return start;
            }
            else {
                h->state = HTTPState::request_done;
                h->do_request = socket_response_recv;
                h->http.tempbuf.size = 0;  // clear tempbuf size
            }
            return start || res;
        }

        bool HTTP::tls_request(HTTP* h, bool start) {
            if (h->state != HTTPState::sending_request) {
                h->err = http_invalid_state;
                return false;
            }
            bool write_done = false;
            auto res = h->conn.tls.write(h->http.tempbuf.text, h->http.tempbuf.size);
            if (!res) {
                if (!h->conn.tls.block()) {
                    h->err = http_send_request_tls;
                    h->state = HTTPState::failed;
                    return false;
                }
            }
            else {
                h->state = HTTPState::request_done;
                h->do_request = tls_response_recv;
                write_done = true;
                h->http.tempbuf.size = 0;  // clear tempbuf size
            }
            if (!do_tls_io_loop(h->conn.sock, h->conn.tls, h->tls.state, h->tls.buf.text, h->tls.buf.size, h->tls.buf.cap)) {
                h->err = http_send_request_tls;
                h->state = HTTPState::failed;
                return false;
            }
            return start || write_done;
        }

        bool HTTP::request(const char* method, const char* path,
                           const void* data, size_t len) {
            if (state != HTTPState::connected && state != HTTPState::tls_connected) {
                err = http_invalid_state;
                return false;
            }
            using namespace net::h1header;
            HTTPBuf requests = {10};
            HTTPPushBack pb{&requests};
            if (!render_request_line(pb, method, path)) {
                err = http_render_request;
                return false;
            }
            if (http.request_header.size == 0) {
                helper::append(pb, "\r\n");
            }
            else {
                helper::append(pb, helper::SizedView(http.request_header.text, http.request_header.size));
            }
            if (data && len) {
                helper::append(pb, helper::SizedView(static_cast<const char*>(data), len));
            }
            http.tempbuf = std::move(requests);
            if (state == HTTPState::tls_connected) {
                do_request = tls_request;
            }
            else {
                do_request = socket_request;
            }
            state = HTTPState::sending_request;
            return do_request(this, true);
        }

        bool HTTP::wait_for_request_done() {
            if (state != HTTPState::sending_request) {
                if (state == HTTPState::request_done) {
                    return true;
                }
                err = http_invalid_state;
                return false;
            }
            return do_request(this, false);
        }

        struct TmpString {
            size_t first = 0;
            size_t end = 0;
            size_t* rptr;
            bool is_value = false;
            const char* text = nullptr;
            char operator[](size_t v) {
                return text[first + v];
            }

            size_t size() const {
                return end - first;
            }

            void push_back(auto&& c) {
                if (is_value) {
                    first = *rptr;
                    end = *rptr + 1;
                    is_value = false;
                    return;
                }
                end++;
            }
        };

        struct TmpHeader {
            void emplace(TmpString&& key, TmpString&& value) {
                auto debug_ = key[0] == value[0];
            }
        };

        auto prepare_TmpString(const char* text, size_t& rptr) {
            return [&, text](TmpString& key, TmpString& value) {
                key.first = rptr;
                key.end = rptr;
                key.rptr = &rptr;
                key.text = text;
                value.is_value = true;
                value.rptr = &rptr;
                value.text = text;
            };
        }

        void shift_space(char* text, HTTPPushBack& to, size_t rptr, size_t remain, size_t& after_size) {
            auto view = helper::SizedView(text, rptr);
            // move data
            helper::append(to, view);
            // shift space
            memmove(text, text + rptr, remain);
            after_size = remain;
        }

        bool HTTP::common_body_recv(HTTP* h) {
            using namespace net::h1body;
            auto seq = make_cpy_seq(helper::SizedView(h->http.tempbuf.text, h->http.tempbuf.size));
            HTTPPushBack pb{&h->http.response_body};
            auto res = read_body(pb, seq, h->http.expect, h->http.btype);
            if (res == net::State::failed) {
                h->state = HTTPState::failed;
                h->err = http_recv_body;
                return false;
            }
            HTTPPushBack bpb{nullptr};
            shift_space(h->http.tempbuf.text, bpb, seq.rptr, seq.remain(), h->http.tempbuf.size);
            if (res == net::State::complete) {
                h->state = HTTPState::response_done;
                return true;
            }
            return false;
        }

        bool HTTP::common_response_recv(HTTP* h) {
            if (h->state == HTTPState::receving_body) {
                return common_body_recv(h);
            }
            using namespace net::h1header;
            using namespace net::h1body;
            HTTPPushBack pb{&h->http.tempbuf};
            auto buf = &h->http.tempbuf;
            auto seq = make_cpy_seq(helper::SizedView(buf->text, buf->size));
            auto read_stopper = [](auto& seq, size_t expect, bool last) { return false; };
            bool begok = false;
            auto res = guess_and_read_raw_header(
                seq, read_stopper, [&](auto& seq) {
                    begok = seq.seek_if("HTTP/1.1 ") || seq.seek_if("HTTP/1.0 ");
                    return begok;
                });
            if (!begok) {
                h->err = http_invalid_response;
                h->state = HTTPState::failed;
                return false;
            }
            if (res) {
                TmpHeader tmp{};
                BodyType btype = BodyType::no_info;
                size_t expect = 0;
                if (!parse_response<TmpString>(
                        seq, helper::nop, h->http.status_code, helper::nop, tmp,
                        bodyinfo_preview(h->http.btype, h->http.expect),
                        prepare_TmpString(pb.buf->text, seq.rptr))) {
                    h->err = http_invalid_response;
                    h->state = HTTPState::failed;
                    return false;
                }
                h->http.response_header = {seq.rptr + 1};
                HTTPPushBack hpb{&h->http.response_header};
                shift_space(pb.buf->text, hpb, seq.rptr, seq.remain(), pb.buf->size);
                h->state = HTTPState::receving_body;
                h->http.response_body = {seq.remain() + 1};
                return common_body_recv(h);
            }
            return false;
        }

        bool HTTP::socket_response_recv(HTTP* h, bool start) {
            char buf[1024];
            bool red = false;
            HTTPPushBack pb{&h->http.tempbuf};
            while (h->conn.sock.read(buf, sizeof(buf))) {
                helper::append(pb, helper::SizedView(buf, h->conn.sock.readsize()));
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
            return common_response_recv(h);
        }

        bool HTTP::tls_response_recv(HTTP* h, bool start) {
            char buf[1024];
            bool red = false;
            HTTPPushBack pb{&h->http.tempbuf};
            while (h->conn.tls.read(buf, sizeof(buf))) {
                helper::append(pb, helper::SizedView(buf, h->conn.tls.readsize()));
                red = true;
            }
            if (!h->conn.tls.block() && !h->conn.tls.closed()) {
                /*TLS::get_errors([](const char* msg, size_t len) {
                    __debugbreak();
                    return 0;
                });*/
                h->err = http_recv_response_tls;
                h->state = HTTPState::failed;
                return false;
            }
            if (!h->conn.tls.closed()) {
                if (!do_tls_io_loop(h->conn.sock, h->conn.tls, h->tls.state, h->tls.buf.text, h->tls.buf.size, h->tls.buf.cap)) {
                    h->err = http_tls_io;
                    h->state = HTTPState::failed;
                    return false;
                }
            }
            if (!red) {
                return false;
            }
            return common_response_recv(h);
        }

        bool HTTP::wait_for_response() {
            if (state != HTTPState::request_done &&
                state != HTTPState::receving_response &&
                state != HTTPState::receving_body) {
                if (state == HTTPState::response_done) {
                    return true;
                }
                err = http_invalid_state;
                return false;
            }
            if (state == HTTPState::request_done) {
                state = HTTPState::receving_response;
            }
            return do_request(this, false);
        }
    }  // namespace dnet
}  // namespace utils
