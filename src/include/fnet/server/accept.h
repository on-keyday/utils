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
    namespace fnet {
        namespace server {

            inline expected<Socket> make_server_socket(SockAddr& addr, int backlog, bool reuse, bool ipv6only) {
                return make_socket(addr.attr)
                    .and_then([&](Socket&& sock) -> expected<Socket> {
                        sock.set_ipv6only(ipv6only);  // ignore errors; best effort
                        sock.set_reuse_addr(reuse);   // ignore errors; best effort
                        return sock;
                    })
                    .and_then([&](Socket&& s) {
                        return s.bind(addr.addr).transform([&] {
                            return std::move(s);
                        });
                    })
                    .and_then([&](Socket&& s) {
                        return s.listen(backlog).transform([&] {
                            return std::move(s);
                        });
                    });
            }

            inline expected<std::pair<Socket, SockAddr>> prepare_listener(view::rvec port, int backlog = 10,
                                                                          bool reuse = true, bool ipv6only = false) {
                return fnet::get_self_server_address(port, fnet::sockattr_tcp())
                    .and_then([&](WaitAddrInfo&& w) {
                        return w.wait();
                    })
                    .and_then([&](AddrInfo&& addr) -> expected<std::pair<Socket, SockAddr>> {
                        error::Error err;
                        while (addr.next()) {
                            auto resolved = addr.sockaddr();
                            auto sock = make_server_socket(resolved, backlog, reuse, ipv6only);
                            if (!sock) {
                                if (err) {
                                    err = error::ErrList{std::move(sock.error()), std::move(err)};
                                }
                                else {
                                    err = std::move(sock.error());
                                }
                            }
                            return std::make_pair(std::move(*sock), std::move(resolved));
                        }
                        if (!err) {
                            return unexpect("no socket address found", error::Category::lib, error::fnet_address_error);
                        }
                        return unexpect(std::move(err));
                    });
            }

        }  // namespace server
    }      // namespace fnet
}  // namespace utils
