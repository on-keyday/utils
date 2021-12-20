/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/tcp/tcp.h"

#include "../../../include/net/core/platform.h"

namespace utils {
    namespace net {
        namespace internal {
            constexpr ::SOCKET invalid_socket = ~0;

            struct TCPImpl {
                wrap::shared_ptr<Address> addr;
                addrinfo* selected = nullptr;
                ::SOCKET sock = invalid_socket;
                bool connected = false;

                void close() {
                    if (sock != invalid_socket) {
                        if (connected) {
                            ::shutdown(sock, SD_BOTH);
                        }
                        ::closesocket(sock);
                        sock = invalid_socket;
                        connected = false;
                    }
                }

                ~TCPImpl() {
                    close();
                }
            };
        }  // namespace internal

        State connecting(::SOCKET& sock) {
            ::timeval timeout = {0};
            ::fd_set baseset = {0}, sucset = {0}, errset = {0};
            FD_ZERO(&baseset);
            FD_SET(sock, &baseset);
            memcpy(&sucset, &baseset, sizeof(::fd_set));
            memcpy(&errset, &baseset, sizeof(::fd_set));
            auto res = ::select(sock + 1, nullptr, &sucset, &errset, &timeout);
            if (res < 0) {
                ::closesocket(sock);
                sock = internal::invalid_socket;
                return State::failed;
            }
            if (FD_ISSET(sock, &errset)) {
                ::closesocket(sock);
                sock = internal::invalid_socket;
                return State::failed;
            }
            if (FD_ISSET(sock, &sucset)) {
                return State::complete;
            }
            return State::running;
        }

        wrap::shared_ptr<TCPConn> TCPResult::connect() {
            if (!impl) {
                return nullptr;
            }
            if (impl->sock == internal::invalid_socket) {
                return nullptr;
            }
            auto make_conn = [&]() {
                auto conn = wrap::make_shared<TCPConn>();
                conn->impl = impl;
                impl = nullptr;
                return conn;
            };
            if (impl->connected) {
                return make_conn();
            }
            auto state = connecting(impl->sock);
            if (state == State::complete) {
                return make_conn();
            }
            return nullptr;
        }

        bool TCPResult::failed() {
            return !impl || impl->sock == internal::invalid_socket;
        }

        TCPResult::~TCPResult() {
            delete impl;
        }

        TCPResult open(wrap::shared_ptr<Address>&& addr) {
            if (!addr) {
                return TCPResult();
            }
            auto info = reinterpret_cast<internal::addrinfo*>(addr->get_rawaddr());
            auto p = info;
            State state = State::failed;
            SOCKET sock = internal::invalid_socket;
            for (; p; p = info->ai_next) {
                auto sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (sock < 0 || sock == internal::invalid_socket) {
                    continue;
                }
                ::u_long nonblock = 1;
                ::ioctlsocket(sock, FIONBIO, &nonblock);
                auto result = ::connect(sock, p->ai_addr, p->ai_addrlen);
                if (result == 0) {
                    break;
                }
                state = connecting(sock);
                if (state == State::failed) {
                    continue;
                }
                break;
            }
            if (!p) {
                return TCPResult();
            }
            TCPResult result;
            result.impl = new internal::TCPImpl();
            result.impl->sock = sock;
            result.impl->connected = state == State::complete;
            result.impl->addr = std::move(addr);
            result.impl->selected = p;
            return result;
        }

    }  // namespace net
}  // namespace utils
