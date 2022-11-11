/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/conn.h>
#include <dnet/dll/sockdll.h>
#include <dnet/dll/httpbufproxy.h>

namespace utils {
    namespace dnet {
        bool Conn::start_resolve(const char* host, const char* port) {
            if (state != ConnState::idle) {
                err = conn_error_invalid_state;
                return false;
            }
            if (!host || !port) {
                err = conn_error_invalid_arguemnt;
                return false;
            }
            SockAddr hint{};
            hint.hostname = host;
            hint.namelen = strlen(host);
            hint.type = SOCK_STREAM;
            wait = resolve_address(hint, port);
            if (wait.failed()) {
                err = conn_error_dns;
                return false;
            }
            state = ConnState::wait_resolve;
            if (wait.wait(addr, 0)) {  // try to resolve immediately
                state = ConnState::resolved;
                return true;
            }
            if (wait.failed()) {
                state = ConnState::failed;
                err = conn_error_wait_dns;
                return false;
            }
            return true;
        }

        bool Conn::wait_for_resolve(std::uint32_t time) {
            if (state != ConnState::wait_resolve) {
                if (state == ConnState::resolved) {
                    return true;
                }
                err = conn_error_invalid_state;
                return false;
            }
            if (wait.wait(addr, time)) {
                state = ConnState::resolved;
                return true;
            }
            if (wait.failed()) {
                state = ConnState::failed;
                err = conn_error_wait_dns;
            }
            return false;
        }

        bool Conn::start_connect() {
            if (state != ConnState::resolved) {
                err = conn_error_invalid_state;
                return false;
            }
            while (addr.next()) {
                SockAddr saddr;
                addr.sockaddr(saddr);
                auto tmp = make_socket(saddr.af, saddr.type, saddr.proto);
                if (!tmp) {
                    continue;
                }
                if (auto err = tmp.connect(saddr.addr, saddr.addrlen)) {
                    if (isBlock(err)) {
                        state = ConnState::start_connect;
                        sock = std::move(tmp);
                        return true;
                    }
                    continue;
                }
                state = ConnState::connected;
                sock = std::move(tmp);
                return true;
            }
            state = ConnState::failed;
            err = conn_error_socket;
            return true;
        }

        bool Conn::wait_for_connect(std::uint32_t sec, std::uint32_t usec) {
            if (state != ConnState::start_connect) {
                if (state == ConnState::connected) {
                    return true;
                }
                err = conn_error_invalid_state;
                return true;
            }
            if (auto e = sock.wait_writable(sec, usec)) {
                if (!dnet::isBlock(e)) {
                    state = ConnState::failed;
                    err = conn_error_socket;
                    return false;
                }
                return false;
            }
            state = ConnState::connected;
            return true;
        }

        bool Conn::set_cert(const char* cacert) {
            if (tls.has_ssl()) {
                return false;
            }
            if (!tls) {
                tls = create_tls();
                if (!tls) {
                    return false;
                }
            }
            return tls.set_cacert_file(cacert);
        }

        bool Conn::start_tls(const char* host, const char* alpn, size_t alpnlen) {
            if (state != ConnState::connected) {
                err = conn_error_invalid_state;
                return false;
            }
            if (!tls) {
                tls = create_tls();
                if (!tls) {
                    state = ConnState::failed;
                    err = conn_error_tls;
                    return false;
                }
            }
            auto rep = [&]() {
                state = ConnState::failed;
                err = conn_error_tls;
                return false;
            };
            if (!tls.has_ssl() && !tls.make_ssl()) {
                return rep();
            }
            if (alpn && alpnlen) {
                if (!tls.set_alpn(alpn, alpnlen)) {
                    return rep();
                }
            }
            if (!tls.set_hostname(host)) {
                return rep();
            }
            if (!tls.connect()) {
                if (tls.block()) {
                    state = ConnState::tls_start;
                    return true;
                }
                return rep();
            }
            state = ConnState::tls_connected;
            return true;
        }

        dnet_dll_implement(bool) do_tls_io_loop(Socket& sock, TLS& tls, TLSIOState& state, char* text, size_t& size, size_t cap) {
            auto do_write = [&] {
                if (auto [_, err] = sock.write(text, size); err) {
                    if (isBlock(err)) {
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
                    error::Error err;
                    while (true) {
                        size_t readsize = 0;
                        std::tie(readsize, err) = sock.read(text, cap);
                        if (err) {
                            break;
                        }
                        size = readsize;
                        if (!do_provide()) {
                            return state != TLSIOState::fatal;
                        }
                        red = true;
                    }
                    if (!red) {
                        count++;
                    }
                    if (err == error::eof) {
                        return true;
                    }
                    if (!isBlock(err)) {
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

        bool Conn::tls_io() {
            HTTPBufProxy p{tls_buf.buf};
            if (!p.text()) {
                p.inisize(1024 * 4);
            }
            if (tls_buf.state == TLSIOState::idle) {
                tls_buf.state = TLSIOState::from_ssl;
            }
            if (!do_tls_io_loop(sock, tls, tls_buf.state, p.text(), p.size(), p.cap())) {
                state = ConnState::failed;
                err = conn_error_tls_socket;
                return false;
            }
            return true;
        }

        bool Conn::wait_for_tls() {
            if (state != ConnState::tls_start) {
                if (state == ConnState::tls_connected) {
                    return true;
                }
                err = conn_error_invalid_state;
                return false;
            }
            if (!tls_io()) {
                return false;
            }
            if (tls.connect()) {
                state = ConnState::tls_connected;
                return true;
            }
            if (!tls.block()) {
                state = ConnState::failed;
                err = conn_error_tls;
            }
            return false;
        }
    }  // namespace dnet
}  // namespace utils
