/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// accept - acceptor
#pragma once
#include "../addrinfo.h"
#include "../socket.h"
#include "../plthead.h"

namespace utils {
    namespace dnet {
        namespace server {

            bool prepare_listeners(const char* port, auto&& prepared, size_t count, int backlog = 10, int* err = nullptr,
                                   bool reuse = true, bool ipv6only = false) {
                if (count == 0) {
                    return false;
                }
                auto wait = dnet::get_self_server_address(port, {.socket_type = SOCK_STREAM});
                if (wait.failed(err)) {
                    return false;
                }
                auto addr = wait.wait();
                if (wait.failed(err)) {
                    return false;
                }
                while (addr.next()) {
                    auto resolved = addr.sockaddr();
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
                if (auto err = sock.wait_readable(0, 1000)) {
                    return failed(err, true /*at wait*/);
                }
                auto [new_sock, addr, err] = sock.accept();
                if (err) {
                    return failed(err, false /*at accept*/);
                }
                accepted(std::move(new_sock), std::move(addr));
                return true;
            }
        }  // namespace server
    }      // namespace dnet
}  // namespace utils
