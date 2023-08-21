/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "error.h"
#include "socket.h"
#include "addrinfo.h"

namespace utils::fnet {
    auto connect(view::rvec hostname, view::rvec port, SockAttr attr, bool wait_write = true) {
        return resolve_address(hostname, port, attr)
            .and_then([](WaitAddrInfo&& info) {
                return info.wait();
            })
            .and_then([&](AddrInfo&& info) -> expected<std::pair<Socket, SockAddr>> {
                while (info.current()) {
                    auto addr = info.sockaddr();
                    auto s = make_socket(addr.attr)
                                 .and_then([&](Socket&& s) {
                                     return s.connect(addr.addr)
                                         .or_else([&](error::Error&& err) -> expected<void> {
                                             if (!isSysBlock(err)) {
                                                 return unexpect(std::move(err));
                                             }
                                             if (wait_write) {
                                                 return s.wait_writable(10, 0);
                                             }
                                             return {};
                                         })
                                         .transform([&] {
                                             return std::make_pair(std::move(s), std::move(addr));
                                         });
                                 });
                    if (s) {
                        return s;
                    }
                }
                return unexpect("cannot connect");
            });
    }
}  // namespace utils::fnet
