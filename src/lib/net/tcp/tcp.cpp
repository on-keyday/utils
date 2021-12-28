/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/tcp/tcp.h"

#include "../../../include/net/core/platform.h"
#include "../../../include/net/core/init_net.h"
#ifdef _WIN32
#define USE_IOCP
#endif
#ifdef USE_IOCP
#include "../../../include/platform/windows/io_completetion_port.h"
#endif

namespace utils {
    namespace net {
        namespace internal {
            constexpr ::SOCKET invalid_socket = ~0;
#ifdef USE_IOCP
            struct TCPIOCP {
                ::WSABUF buf;
                char buffer[1024] = {0};
                ::OVERLAPPED ol;
                bool iocprunning = false;
                bool done = false;
                size_t size = 0;
                wrap::weak_ptr<TCPConn> self;
            };
#endif
            struct TCPImpl {
                wrap::shared_ptr<Address> addr;
                addrinfo* selected = nullptr;
                ::SOCKET sock = invalid_socket;
                bool connected = false;
#ifdef USE_IOCP
                TCPIOCP iocp;
#endif
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

        size_t TCPConn::get_raw() const {
            if (!impl) {
                return internal::invalid_socket;
            }
            return impl->sock;
        }

#ifdef USE_IOCP
        auto io_complete_callback(wrap::weak_ptr<TCPConn>& self, internal::TCPImpl* impl) {
            return [=](size_t size) {
                auto conn = self.lock();
                if (!conn) {
                    return;
                }
                impl->iocp.size += size;
                impl->iocp.done = true;
            };
        }

        State handle_iocp(internal::TCPImpl* impl, char* ptr, size_t size, size_t* red) {
        BEGIN:
            if (impl->iocp.iocprunning) {
                if (impl->iocp.done) {
                    auto cpy = size >= impl->iocp.size ? impl->iocp.size : size;
                    ::memcpy(ptr, impl->iocp.buffer, cpy);
                    impl->iocp.size -= cpy;
                    ::memmove(impl->iocp.buffer, impl->iocp.buffer + cpy, cpy);
                    *red = cpy;
                    if (impl->iocp.size == 0) {
                        impl->iocp.done = false;
                        impl->iocp.size = 0;
                        ::CloseHandle(impl->iocp.ol.hEvent);
                        impl->iocp.iocprunning = false;
                    }
                    return State::complete;
                }
                return State::running;
            }
            impl->iocp.buf.buf = impl->iocp.buffer;
            impl->iocp.buf.len = 1024;
            impl->iocp.iocprunning = true;
            impl->iocp.ol = {};
            impl->iocp.ol.hEvent = ::CreateEventW(nullptr, true, false, nullptr);
            DWORD flagset = 0;
            auto res = ::WSARecv(impl->sock, &impl->iocp.buf, 1, nullptr, &flagset, &impl->iocp.ol, nullptr);
            auto err = ::WSAGetLastError();
            if (err != NO_ERROR && err != WSA_IO_PENDING) {
                return State::failed;
            }
            if (err == NO_ERROR) {
                goto BEGIN;
            }
            return State::running;
        }
#endif

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
#ifdef USE_IOCP
            return handle_iocp(impl, ptr, size, red);
#else
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
#endif
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

        State TCPConn::close(bool) {
            if (impl) {
                impl->close();
            }
            return State::complete;
        }

        State wait_connect(::SOCKET& sock, bool server = false, long sec = 0) {
            ::timeval timeout = {0};
            ::fd_set baseset = {0}, sucset = {0}, errset = {0};
            FD_ZERO(&baseset);
            FD_SET(sock, &baseset);
            memcpy(&sucset, &baseset, sizeof(::fd_set));
            memcpy(&errset, &baseset, sizeof(::fd_set));
            timeout.tv_usec = 1;
            timeout.tv_sec = sec;
            int res = 0;
            if (server) {
                res = ::select(sock + 1, &sucset, nullptr, &errset, &timeout);
            }
            else {
                res = ::select(sock + 1, nullptr, &sucset, &errset, &timeout);
            }
            if (res < 0) {
                if (!server) {
                    ::closesocket(sock);
                    sock = internal::invalid_socket;
                }
                return State::failed;
            }
            if (FD_ISSET(sock, &errset)) {
                if (!server) {
                    ::closesocket(sock);
                    sock = internal::invalid_socket;
                }
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
#ifdef USE_IOCP
                conn->impl->iocp.self = conn;
                auto iocp = platform::windows::start_iocp();
                auto s = iocp->register_handler(reinterpret_cast<void*>(conn->impl->sock),
                                                io_complete_callback(conn->impl->iocp.self, conn->impl));
                assert(s);
#endif
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
            network();
            if (!addr) {
                return TCPResult();
            }
            auto info = reinterpret_cast<internal::addrinfo*>(addr->get_rawaddr());
            auto p = info;
            State state = State::failed;
            SOCKET sock = internal::invalid_socket;
            for (; p; p = info->ai_next) {
#ifdef _WIN32
                sock = ::WSASocketW(p->ai_family, p->ai_socktype, p->ai_protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
#else
                sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#endif
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

        bool TCPServer::wait(long sec = 60) {
            return wait_connect(impl->sock, true, sec) == State::complete;
        }

        wrap::shared_ptr<TCPConn> TCPServer::accept() {
            if (!impl) {
                return nullptr;
            }
            auto e = wait_connect(impl->sock, true);
            if (e != State::complete) {
                return nullptr;
            }
            ::sockaddr_storage st;
            int size = sizeof(st);
            auto acsock = ::accept(impl->sock, reinterpret_cast<::sockaddr*>(&st), &size);
            if (acsock < 0 || acsock == internal::invalid_socket) {
                return nullptr;
            }
            auto addr = internal::from_sockaddr_st(reinterpret_cast<void*>(&st), size);
            auto conn = wrap::make_shared<TCPConn>();
            conn->impl->addr = std::move(addr);
            conn->impl->sock = acsock;
            conn->impl->connected = true;
            return conn;
        }

        TCPServer::TCPServer(TCPServer&& in) {
            impl = in.impl;
            in.impl = nullptr;
        }

        TCPServer& TCPServer::operator=(TCPServer&& in) {
            delete impl;
            impl = in.impl;
            in.impl = nullptr;
            return *this;
        }

        TCPServer::~TCPServer() {
            delete impl;
        }

        TCPServer setup(wrap::shared_ptr<Address>&& addr, int ipver) {
            network();
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
            result.impl->sock = sock;
            result.impl->selected = p;
            result.impl->connected = true;
            result.impl->addr = std::move(addr);
            return result;
        }

    }  // namespace net
}  // namespace utils
