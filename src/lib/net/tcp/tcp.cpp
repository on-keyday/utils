/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/tcp/tcp.h"

#include "../../../include/net/core/platform.h"
#include "../../../include/net/core/init_net.h"
#ifdef _WIN32
//#define USE_IOCP
#endif
//#ifdef USE_IOCP
#include "../../../include/platform/windows/io_completetion_port.h"
//#endif

#include "../../../include/net/async/pool.h"

namespace utils {
    namespace net {
        namespace internal {
            constexpr ::SOCKET invalid_socket = ~0;
#ifdef _WIN32
            struct TCPIOCP {
                ::OVERLAPPED ol{0};
                platform::windows::CompletionType type = platform::windows::CompletionType::tcp_read;
                ReadInfo* info = nullptr;
                async::AnyFuture f;
                wrap::weak_ptr<TCPConn> conn;
                bool already_set = false;
            };

            bool tcp_callback() {
                auto iocp = platform::windows::get_iocp();
                iocp->register_callback([](void* ol, size_t t) {
                    auto ptr = static_cast<TCPIOCP*>(ol);
                    if (ptr->type != platform::windows::CompletionType::tcp_read) {
                        return false;
                    }
                    auto req = ptr->f.get_taskrequest();
                    if (ptr->info) {
                        ptr->info->read = t;
                    }
                    req->complete();
                    return true;
                });
                return true;
            }

            void tcp_iocp_init(SOCKET sock, TCPIOCP* ptr) {
                static bool ini = tcp_callback();
                if (ptr && !ptr->already_set) {
                    platform::windows::get_iocp()->register_handle(reinterpret_cast<void*>(sock));
                    ptr->already_set = true;
                }
            }
#endif

            struct TCPImpl {
                wrap::shared_ptr<Address> addr;
                addrinfo* selected = nullptr;
                ::SOCKET sock = invalid_socket;
                bool connected = false;
#ifdef _WIN32
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
#define WSAEWOULDBLOCK EWOULDBLOCK
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

        async::Future<ReadInfo> TCPConn::read(char* ptr, size_t size) {
            if (!ptr || !size) {
                return nullptr;
            }
            return get_pool().start<ReadInfo>([=, this, lock = impl->iocp.conn.lock()](async::Context& ctx) {
#ifdef _WIN32
                ::WSABUF buf;
                buf.buf = ptr;
                buf.len = size;
                ::DWORD red;
                internal::tcp_iocp_init(impl->sock, &impl->iocp);
                ReadInfo info;
                info.byte = ptr;
                info.size = size;
                impl->iocp.info = &info;
                impl->iocp.f = ctx.clone();
                auto res = ::WSARecv(impl->sock, &buf, 1, &red, 0, &impl->iocp.ol, nullptr);
                if (res == SOCKET_ERROR) {
                    auto err = ::GetLastError();
                    if (err == WSA_IO_PENDING) {
                        ctx.externaltask_wait();
                    }
                    else {
                        info.err = err;
                        ctx.set_value(info);
                        return;
                    }
                }
                impl->iocp.info = nullptr;
                impl->iocp.f.clear();
                ctx.set_value(info);
#endif
            });
        }

        async::Future<WriteInfo> TCPConn::write(const char* ptr, size_t size) {
            if (!ptr || !size) {
                return nullptr;
            }
            auto t = write(ptr, size, nullptr);
            while (t != State::complete) {
                t = write(ptr, size, nullptr);
            }
            if (t == State::failed) {
                return async::Future{WriteInfo{.byte = ptr, .size = size, .err = (int)::GetLastError()}};
            }
            return async::Future{WriteInfo{.byte = ptr, .size = size, .written = size}};
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

        State TCPConn::write(const char* ptr, size_t size, size_t* written) {
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
            if (written) {
                *written = size;
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
                conn->impl->connected = true;
#ifdef _WIN32
                conn->impl->iocp.conn = conn;
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
            ::socklen_t size = sizeof(st);
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
