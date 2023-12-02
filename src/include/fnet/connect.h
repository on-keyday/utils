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
    struct ConnectError {
        error::Error err;

        void error(auto&& p) {
            strutil::append(p, "connect error: ");
            err.error(p);
        }
    };

    auto connect(view::rvec hostname, view::rvec port, SockAttr attr, bool wait_write = true) {
        return resolve_address(hostname, port, attr)
            .and_then([](WaitAddrInfo&& info) {
                return info.wait();
            })
            .and_then([&](AddrInfo&& info) -> expected<std::pair<Socket, SockAddr>> {
                error::Error err;
                while (info.next()) {
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
                    if (err) {
                        err = error::ErrList{std::move(s.error()), std::move(err)};
                    }
                    else {
                        err = std::move(s.error());
                    }
                }
                if (err) {
                    return unexpect(ConnectError{std::move(err)});
                }
                return unexpect(ConnectError{error::Error("cannot connect", error::Category::lib, error::fnet_network_error)});
            });
    }

    enum class WithState {
        addr_solve,
        connect,
    };

    auto connect_with(view::rvec hostname, view::rvec port, SockAttr attr, bool wait_write, auto&& with) {
        return resolve_address(hostname, port, attr)
            .and_then([&](WaitAddrInfo&& info) {
                return with(WithState::addr_solve, [&](auto t, std::uint32_t = 0) {
                    return info.wait(t);
                });
            })
            .and_then([&](AddrInfo&& info) -> expected<std::pair<Socket, SockAddr>> {
                while (info.next()) {
                    auto addr = info.sockaddr();
                    auto s = make_socket(addr.attr)
                                 .and_then([&](Socket&& s) {
                                     return s.connect(addr.addr)
                                         .or_else([&](error::Error&& err) -> expected<void> {
                                             if (!isSysBlock(err)) {
                                                 return unexpect(std::move(err));
                                             }
                                             if (wait_write) {
                                                 return with(WithState::connect, [&](auto t, std::uint32_t us = 0) {
                                                     return s.wait_writable(t, us);
                                                 });
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
                return unexpect("cannot connect", error::Category::lib, error::fnet_network_error);
            });
    }
}  // namespace utils::fnet
