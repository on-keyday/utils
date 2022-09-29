/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// accept - acceptor
#pragma once
#include "../addrinfo.h"
#include "../conn.h"
#include "../plthead.h"

namespace utils {
    namespace dnet {
        namespace server {

            bool prepare_listeners(const char* port, auto&& prepared, size_t count, int backlog = 10, int* err = nullptr,
                                   bool reuse = true, bool ipv6only = false) {
                if (count == 0) {
                    return false;
                }
                auto wait = dnet::get_self_server_address({.type = SOCK_STREAM}, port);
                if (wait.failed(err)) {
                    return false;
                }
                auto addr = wait.wait();
                if (wait.failed(err)) {
                    return false;
                }
                while (addr.next()) {
                    SockAddr resolved;
                    addr.sockaddr(resolved);
                    auto sock = make_server_socket(resolved, backlog, reuse, ipv6only);
                    if (!sock) {
                        continue;
                    }
                    prepared(std::as_const(addr), sock);
                    for (size_t i = 1; i < count; i++) {
                        sock = make_server_socket(resolved, backlog, reuse, ipv6only);
                        if (!sock) {
                            return false;
                        }
                        prepared(std::as_const(addr), sock);
                    }
                    return true;
                }
                return false;
            }

            bool do_accept(Socket& sock, auto&& accepted, auto&& failed) {
                if (!sock.wait_readable(0, 1000)) {
                    return failed(true /*at wait*/);
                }
                ::sockaddr_storage st;
                int len = sizeof(st);
                dnet::Socket new_sock;
                if (!sock.accept(new_sock, &st, &len)) {
                    return failed(false /*at accept*/);
                }
                int port = 0;
                auto ipaddr = string_from_sockaddr(&st, len, &port);
                accepted(new_sock, ipaddr, port);
                return true;
            }
        }  // namespace server
    }      // namespace dnet
}  // namespace utils
