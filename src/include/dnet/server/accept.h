/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// accept - acceptor
#pragma once
#include "../addrinfo.h"
#include "../socket.h"

namespace utils {
    namespace dnet {
        namespace server {

            inline std::pair<Socket, error::Error> make_server_socket(SockAddr& addr, int backlog, bool reuse, bool ipv6only) {
                auto sock = make_socket(addr.attr);
                if (!sock) {
                    return {
                        Socket{},
                        error::Error("make_socket failed"),
                    };
                }
                sock.set_ipv6only(ipv6only);
                sock.set_reuse_addr(reuse);
                if (auto err = sock.bind(addr.addr)) {
                    return {Socket{}, std::move(err)};
                }
                if (auto err = sock.listen(backlog)) {
                    return {Socket{}, std::move(err)};
                }
                return {std::move(sock), error::none};
            }

            bool prepare_listeners(const char* port, auto&& prepared, size_t count, int backlog = 10, int* err = nullptr,
                                   bool reuse = true, bool ipv6only = false) {
                if (count == 0) {
                    return false;
                }
                auto wait = dnet::get_self_server_address(port, dnet::sockattr_tcp());
                if (wait.failed(err)) {
                    return false;
                }
                auto addr = wait.wait();
                if (wait.failed(err)) {
                    return false;
                }
                while (addr.next()) {
                    auto resolved = addr.sockaddr();
                    auto [sock, err] = make_server_socket(resolved, backlog, reuse, ipv6only);
                    if (err) {
                        continue;
                    }
                    prepared(std::as_const(resolved.addr), sock);
                    for (size_t i = 1; i < count; i++) {
                        std::tie(sock, err) = make_server_socket(resolved, backlog, reuse, ipv6only);
                        if (!sock) {
                            return false;
                        }
                        prepared(std::as_const(resolved.addr), sock);
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
