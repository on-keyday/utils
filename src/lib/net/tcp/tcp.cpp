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

                bool is_closed() const {
                    return sock == invalid_socket;
                }

                ~TCPImpl() {
                    close();
                }
            };
        }  // namespace internal

        static bool is_blocking() {
#ifdef _WIN32
            auto err = ::WSAGetLastError();
#else
            auto err = errno;
#endif
            if (err == WSAEWOULDBLOCK) {
                return true;
            }
            return false;
        }

        TCPResult::TCPResult(TCPResult&& in) {
            impl = in.impl;
            in.impl = nullptr;
        }

        TCPResult& TCPResult::operator=(TCPResult&& in) {
            delete impl;
            impl = in.impl;
            in.impl = nullptr;
            return *this;
        }

        State TCPConn::read(char* ptr, size_t size, size_t* red) {
            if (!impl || impl->is_closed()) {
                return State::failed;
            }
            if (!ptr || !size) {
                return State::invalid_argument;
            }
            if (size > (std::numeric_limits<int>::max)()) {
                size = (std::numeric_limits<int>::max)();
            }
            auto res = ::recv(impl->sock, ptr, int(size), 0);
            if (res < 0) {
                if (is_blocking()) {
                    return State::running;
                }
                return State::failed;
            }
            if (red) {
                *red = size_t(res);
            }
            return State::complete;
        }

        State TCPConn::write(const char* ptr, size_t size) {
            if (!impl || impl->is_closed()) {
                return State::failed;
            }
            if (!ptr || !size || size > (std::numeric_limits<int>::max)()) {
                return State::invalid_argument;
            }
            int res = ::send(impl->sock, ptr, int(size), 0);
            if (res < 0) {
                if (is_blocking()) {
                    return State::running;
                }
                return State::failed;
            }
            return State::complete;
        }

        void TCPConn::close() {
            if (impl) {
                impl->close();
            }
        }

        State wait_connect(::SOCKET& sock) {
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
            if (!impl || impl->is_closed()) {
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
            auto state = wait_connect(impl->sock);
            if (state == State::complete) {
                return make_conn();
            }
            return nullptr;
        }

        bool TCPResult::failed() {
            return !impl || impl->is_closed();
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
                state = wait_connect(sock);
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

        wrap::shared_ptr<TCPConn> TCPServer::accept() {
        }

        TCPServer setup(wrap::shared_ptr<Address>&& addr, int ipver) {
            if (!addr) {
                return TCPServer();
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
                u_long flag = 1;
                if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) < 0) {
                    ::closesocket(sock);
                    continue;
                }
                ::ioctlsocket(sock, FIONBIO, &nonblock);
                if (ipver == 4) {
                    p->ai_addr->sa_family = AF_INET;
                }
                else {
                    if (ipver == 6) {
                        flag = 0;
                    }
                    if (::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag)) < 0) {
                        ::closesocket(sock);
                        continue;
                    }
                }
                auto res = ::bind(sock, p->ai_addr, p->ai_addrlen);
                if (res < 0) {
                    ::closesocket(sock);
                    continue;
                }
                res = ::listen(sock, 5);
                if (res < 0) {
                    ::closesocket(sock);
                    continue;
                }
                break;
            }
            TCPServer result;
            result.impl = new internal::TCPImpl();
        }

    }  // namespace net
}  // namespace utils
