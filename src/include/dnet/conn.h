/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn - connection suite
#pragma once
#include "dll/dllh.h"
#include "addrinfo.h"
#include "socket.h"
#include "tls.h"
#include "dll/httpbuf.h"

namespace utils {
    namespace dnet {
        enum class ConnState {
            idle,
            wait_resolve,
            resolved,
            start_connect,
            connected,
            tls_start,
            tls_connected,
            failed,
        };

        enum ConnError {
            conn_error_none,
            conn_error_dns,
            conn_error_wait_dns,
            conn_error_socket,
            conn_error_tls,
            conn_error_tls_socket,
            conn_error_invalid_state,
            conn_error_invalid_arguemnt,
            conn_error_max,
        };

        struct ConnConfig {
            const char* host = nullptr;
            const char* port = nullptr;
            std::uint32_t dns_timeout = 0;
            struct {
                std::uint32_t sec;
                std::uint32_t usec;
            } connect_timeout{0, 0};
            const char* cacert = nullptr;
            const char* alpn = nullptr;
            size_t alpnlen = 0;
        };

        // Conn is transport layer connection suite
        // resolvement wrapper of DNS, TCP/UDP and TLS
        struct dnet_class_export Conn {
            AddrInfo addr;
            WaitAddrInfo wait;
            Socket sock;
            TLS tls;
            struct {
                HTTPBuf buf;
                TLSIOState state = TLSIOState::idle;
            } tls_buf;
            ConnState state = ConnState::idle;
            int err = conn_error_none;
            bool start_resolve(const char* host, const char* port);
            bool wait_for_resolve(std::uint32_t msec);
            bool start_connect();
            bool wait_for_connect(std::uint32_t sec, std::uint32_t usec);
            bool set_cert(const char* cacert);
            bool start_tls(const char* host, const char* alpn, size_t alpnlen);
            bool wait_for_tls();
            bool tls_io();
            constexpr bool failed() const {
                return state == ConnState::failed;
            }

            bool establish_connection(ConnConfig config, bool tls) {
                if (state == ConnState::idle) {
                    if (!start_resolve(config.host, config.port)) {
                        return false;
                    }
                }
                if (state == ConnState::wait_resolve) {
                    if (!wait_for_resolve(config.dns_timeout)) {
                        return false;
                    }
                }
                if (state == ConnState::resolved) {
                    if (!start_connect()) {
                        return false;
                    }
                }
                if (state == ConnState::start_connect) {
                    if (!wait_for_connect(config.connect_timeout.sec, config.connect_timeout.usec)) {
                        return false;
                    }
                }
                if (state == ConnState::connected) {
                    if (!tls) {
                        return true;
                    }
                    set_cert(config.cacert);
                    if (!start_tls(config.host, config.alpn, config.alpnlen)) {
                        return false;
                    }
                }
                if (state == ConnState::tls_start) {
                    if (!wait_for_tls()) {
                        return false;
                    }
                }
                if (state == ConnState::tls_connected) {
                    return true;
                }
                state = ConnState::failed;
                if (err == conn_error_none) {
                    err = conn_error_invalid_state;
                }
                return false;
            }

            bool write(const char* data, size_t len) {
                if (!sock) {
                    return false;
                }
                if (tls) {
                    return tls.write(data, len).is_noerr() && tls_io();
                }
                return !sock.write(view::rvec(data, len)).second.is_error();
            }

            bool read(char* data, size_t len, size_t& red) {
                if (!sock) {
                    return false;
                }
                if (tls) {
                    auto res = tls_io() && tls.read(data, len);
                    red = tls.readsize();
                    return res;
                }
                error::Error res;
                view::wvec red_v;
                std::tie(red_v, res) = sock.read(view::wvec(data, len));
                red = red_v.size();
                return !res.is_error();
            }
        };
        dnet_dll_export(bool) do_tls_io_loop(Socket& sock, TLS& tls, TLSIOState& state, char* text, size_t& size, size_t cap);

        inline Socket make_server_socket(const SockAddr& resolved, int backlog = 10, bool reuse = true, bool v6only = false) {
            auto sock = dnet::make_socket(resolved.attr.address_family, resolved.attr.socket_type, resolved.attr.protocol);
            if (!sock ||
                sock.set_reuse_addr(reuse).is_error() ||
                sock.set_ipv6only(v6only).is_error() ||
                sock.bind(resolved.addr).is_error() ||
                sock.listen(backlog).is_error()) {
                return {};
            }
            return sock;
        }
    }  // namespace dnet
}  // namespace utils
