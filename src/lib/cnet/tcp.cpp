/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/cnet/tcp.h"

#include <cstdint>
#include <atomic>
#include "../../include/number/array.h"
#include "../../include/wrap/light/char.h"
#include "../../include/helper/appender.h"
#include "sock_inc.h"

namespace utils {
    namespace cnet {
        namespace tcp {
            template <class Char>
            using Host = number::Array<254, Char, true>;
            template <class Char>
            using Port = number::Array<10, Char, true>;

            enum class Flag {
                none,
                retry_after_connect = 0x1,
                multiple_io = 0x2,
                poll_recv = 0x4,

                made_from_sockaddr_storage = 0x8,
            };

            DEFINE_ENUM_FLAGOP(Flag)

            constexpr std::uint32_t reserved_byte = 0x434e5450;

            struct MultiIOState {
#ifdef _WIN32
                ::OVERLAPPED ol{0};
#endif
                std::uint32_t reserved = reserved_byte;
                size_t recvsize = 0;
                std::atomic_bool done = false;
            };

            struct OsTCPSocket {
                ::SOCKET sock = -1;
                Host<wrap::path_char> host{0};
                Port<wrap::path_char> port{0};
                int ipver = 0;
                ::ADDRINFOEXW* info = nullptr;
                ::ADDRINFOEXW* selected = nullptr;
                Flag flag = Flag::none;
                TCPStatus status = TCPStatus::idle;
            };

            ADDRINFOEXW* dns_query(CNet* ctx, OsTCPSocket* sock) {
                sock->status = TCPStatus::start_resolving_name;
                invoke_callback(ctx);
                ADDRINFOEXW info{0}, *result = nullptr;
                if (sock->ipver == 4) {
                    info.ai_family = AF_INET;
                }
                else if (sock->ipver == 6) {
                    info.ai_family = AF_INET6;
                }
                ::OVERLAPPED ol{0};
                ol.hEvent = ::CreateEventA(nullptr, true, false, nullptr);
                if (ol.hEvent == nullptr) {
                    return nullptr;
                }
                ::timeval timeout{0};
                ::HANDLE cancel = nullptr;
                timeout.tv_sec = 60;
                ::GetAddrInfoExW(
                    sock->host.c_str(), sock->port.c_str(),
                    NS_DNS, nullptr, &info, &result, &timeout, &ol, nullptr, &cancel);
                auto err = net::errcode();
                if (err != NO_ERROR && err != WSA_IO_PENDING) {
                    return nullptr;
                }
                while (err != NO_ERROR) {
                    err = ::GetAddrInfoExOverlappedResult(&ol);
                    if (err != WSAEINPROGRESS) {
                        ::CloseHandle(ol.hEvent);
                        if (err == NO_ERROR) {
                            break;
                        }
                        return nullptr;
                    }
                    sock->status = TCPStatus::wait_resolving_name;
                    invoke_callback(ctx);
                }
                sock->status = TCPStatus::resolve_name_done;
                invoke_callback(ctx);
                return result;
            }

            bool selecting_loop(CNet* ctx, ::SOCKET proto, bool send, TCPStatus& status) {
                ::fd_set base{0};
                FD_ZERO(&base);
                FD_SET(proto, &base);
                while (true) {
                    ::fd_set suc = base, fail = base;
                    ::timeval tv{0};
                    if (send) {
                        ::select(proto + 1, nullptr, &suc, &fail, &tv);
                        status = TCPStatus::wait_connect;
                    }
                    else {
                        ::select(proto + 1, &suc, nullptr, &fail, &tv);
                        if (status != TCPStatus::wait_accept) {
                            status = TCPStatus::wait_recv;
                        }
                    }
                    if (FD_ISSET(proto, &suc)) {
                        return true;
                    }
                    else if (FD_ISSET(proto, &fail)) {
                        return false;
                    }
                    invoke_callback(ctx);
                }
            }

            bool open_socket(CNet* ctx, OsTCPSocket* sock) {
                if (!net::network().initialized()) {
                    return false;
                }
                sock->info = dns_query(ctx, sock);
                if (!sock->info) {
                    return false;
                }
                sock->status = TCPStatus::start_connectig;
                invoke_callback(ctx);
                for (auto p = sock->info; p; p = p->ai_next) {
#ifdef _WIN32
                    auto proto = ::WSASocketW(p->ai_family, p->ai_socktype, p->ai_protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
#else
                    auto proto = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#endif
                    if (proto < 0 || proto == -1) {
                        continue;
                    }
                    u_long nonblock = 1;
                    ::ioctlsocket(proto, FIONBIO, &nonblock);
                    auto err = ::connect(proto, p->ai_addr, p->ai_addrlen);
                    if (err == 0) {
                        sock->sock = proto;
                        sock->selected = p;
                        break;
                    }
                    if (net::errcode() == WSAEWOULDBLOCK) {
                        sock->sock = proto;
                        if (selecting_loop(ctx, sock->sock, true, sock->status)) {
                            sock->selected = p;
                            break;
                        }
                    }
                    sock->sock = -1;
                    ::closesocket(proto);

                    if (any(sock->flag & Flag::retry_after_connect)) {
                        continue;
                    }
                    return false;
                }
                sock->status = TCPStatus::connected;
                invoke_callback(ctx);
                return true;
            }

            void close_socket(CNet* ctx, OsTCPSocket* sock) {
                if (any(sock->flag & Flag::made_from_sockaddr_storage)) {
                    delete (::sockaddr_storage*)sock->info->ai_addr;
                    delete sock->info;
                }
                else {
                    ::FreeAddrInfoExW(sock->info);
                }
                sock->info = nullptr;
                sock->selected = nullptr;
                ::closesocket(sock->sock);
                sock->sock = -1;
            }

            bool write_socket(CNet* ctx, OsTCPSocket* sock, Buffer<const char>* buf) {
                sock->status = TCPStatus::start_sending;
                invoke_callback(ctx);
                auto err = ::send(sock->sock, buf->ptr, buf->size, 0);
                if (err < 0) {
                    if (net::errcode() == WSAEWOULDBLOCK) {
                        return true;
                    }
                    return false;
                }
                buf->proced = err;
                sock->status = TCPStatus::sent;
                invoke_callback(ctx);
                return true;
            }

            bool STDCALL io_completion(void* ol, size_t recvsize) {
                if (!ol) {
                    return false;
                }
                auto check = static_cast<MultiIOState*>(ol);
                if (check->reserved != reserved_byte) {
                    return false;
                }
                check->recvsize = recvsize;
                check->done = true;
                return true;
            }

            bool read_socket(CNet* ctx, OsTCPSocket* sock, Buffer<char>* buf) {
                sock->status = TCPStatus::start_recving;
                invoke_callback(ctx);
                if (any(sock->flag & Flag::multiple_io)) {
                    MultiIOState state;
                    state.done = false;
                    state.reserved = reserved_byte;
                    state.recvsize = 0;
#ifdef _WIN32
                    WSABUF wbuf;
                    wbuf.buf = buf->ptr;
                    wbuf.len = buf->size;
                    DWORD recvd = 0, flags = 0;
                    auto err = ::WSARecv(sock->sock, &wbuf, 1, &recvd, &flags, &state.ol, nullptr);
                    if (err == SOCKET_ERROR) {
                        auto err = net::errcode();
                        if (err != WSA_IO_PENDING) {
                            return false;
                        }
                    }
                    while (!state.done) {
                        sock->status = TCPStatus::wait_async_recv;
                        invoke_callback(ctx);
                    }
                    buf->proced = state.recvsize;
#endif
                }
                else {
                    while (true) {
                        auto err = ::recv(sock->sock, buf->ptr, buf->size, 0);
                        if (err < 0) {
                            if (net::errcode() == WSAEWOULDBLOCK) {
                                if (any(sock->flag & Flag::poll_recv)) {
                                    if (!selecting_loop(ctx, sock->sock, false, sock->status)) {
                                        return false;
                                    }
                                    sock->status = TCPStatus::wait_recv;
                                    invoke_callback(ctx);
                                    continue;
                                }
                                return true;
                            }
                            return false;
                        }
                        buf->proced = err;
                        break;
                    }
                }
                sock->status = TCPStatus::recieved;
                invoke_callback(ctx);
                return true;
            }

            bool tcp_settings_number(CNet* ctx, OsTCPSocket* sock, std::int64_t key, std::int64_t value) {
                auto set = [&](Flag v) {
                    if (value) {
                        sock->flag |= v;
                    }
                    else {
                        sock->flag &= ~v;
                    }
                };
                if (key == retry_after_connect_call) {
                    set(Flag::retry_after_connect);
                }
                else if (key == use_multiple_io_system) {
                    set(Flag::multiple_io);
                }
                else if (key == recv_polling) {
                    set(Flag::poll_recv);
                }
                else if (key == ipver) {
                    sock->ipver = value;
                }
                else {
                    return false;
                }
                return true;
            }

            bool tcp_settings_ptr(CNet* ctx, OsTCPSocket* sock, std::int64_t key, void* value) {
                if (!value) {
                    return false;
                }
                if (key == host_name) {
                    auto host = (const char*)value;
                    sock->host = {};
                    helper::append(sock->host, host);
                }
                else if (key == port_number) {
                    auto port = (const char*)value;
                    sock->port = {};
                    helper::append(sock->port, port);
                }
                else {
                    return false;
                }
                return true;
            }

            std::int64_t tcp_query_number(CNet* ctx, OsTCPSocket* sock, std::int64_t key) {
                if (key == protocol_type) {
                    return byte_stream;
                }
                else if (key == error_code) {
                    return net::errcode();
                }
                else if (key == ipver) {
                    return sock->ipver;
                }
                else if (key == raw_socket) {
                    return sock->sock;
                }
                else if (key == current_status) {
                    return (std::int64_t)sock->status;
                }
                else if (key == retry_after_connect_call) {
                    return any(sock->flag & Flag::retry_after_connect);
                }
                else if (key == use_multiple_io_system) {
                    return any(sock->flag & Flag::multiple_io);
                }
                else if (key == recv_polling) {
                    return any(sock->flag & Flag::poll_recv);
                }
                return 0;
            }

            void* tcp_query_ptr(CNet* ctx, OsTCPSocket* sock, std::int64_t key) {
                if (key == protocol_name) {
                    return (void*)"tcp";
                }
                else if (key == host_name) {
                    return (void*)sock->host.c_str();
                }
                else if (key == port_number) {
                    return (void*)sock->port.c_str();
                }
                return nullptr;
            }

            CNet* STDCALL create_client() {
                ProtocolSuite<OsTCPSocket> tcp_proto{
                    .initialize = open_socket,
                    .write = write_socket,
                    .read = read_socket,
                    .uninitialize = close_socket,
                    .settings_number = tcp_settings_number,
                    .settings_ptr = tcp_settings_ptr,
                    .query_number = tcp_query_number,
                    .query_ptr = tcp_query_ptr,
                    .deleter = [](OsTCPSocket* sock) { delete sock; },
                };
                auto ctx = create_cnet(CNetFlag::final_link | CNetFlag::init_before_io, new OsTCPSocket{}, tcp_proto);
                return ctx;
            }

            CNet* create_server_conn(::SOCKET sock, ::sockaddr_storage* info, size_t len) {
                ProtocolSuite<OsTCPSocket> tcp_proto{
                    .write = write_socket,
                    .read = read_socket,
                    .uninitialize = close_socket,
                    .settings_number = tcp_settings_number,
                    .settings_ptr = tcp_settings_ptr,
                    .query_number = tcp_query_number,
                    .query_ptr = tcp_query_ptr,
                    .deleter = [](OsTCPSocket* sock) { delete sock; },
                };
                auto ptr = new OsTCPSocket{};
                ptr->sock = sock;
                auto addr = new ::ADDRINFOEXW{};
                addr->ai_addr = (::sockaddr*)new ::sockaddr_storage{*info};
                addr->ai_addrlen = len;
                addr->ai_family = info->ss_family;
                ptr->info = addr;
                ptr->selected = addr;
                ptr->flag |= Flag::made_from_sockaddr_storage;
                auto ctx = create_cnet(CNetFlag::final_link | CNetFlag::init_before_io, ptr, tcp_proto);
                return ctx;
            }

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
