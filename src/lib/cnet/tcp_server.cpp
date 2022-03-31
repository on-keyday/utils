/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/cnet/tcp.h"
#include "sock_inc.h"
#include "../../include/number/array.h"
#include "../../include/helper/appender.h"

namespace utils {
    namespace cnet {
        namespace tcp {

            enum class ServFlag {
                none,
                reuse_addr,
            };

            DEFINE_ENUM_FLAGOP(ServFlag)

            struct TCPServer {
                ::SOCKET litener = -1;
                TCPStatus status = TCPStatus::idle;
                number::Array<10, char, true> port{};
                ::addrinfo* info = nullptr;
                ::addrinfo* selected = nullptr;
                int ipver = 0;
                ServFlag flag = ServFlag::none;
                std::int64_t timeout = -1;
            };

            bool tcp_server_init(CNet* ctx, TCPServer* serv) {
                if (!net::network().initialized()) {
                    return false;
                }
                ::addrinfo hint{0}, *result = nullptr;
                if (serv->ipver == 4) {
                    hint.ai_family = AF_INET;
                }
                else {
                    hint.ai_family = AF_INET6;
                }
                hint.ai_flags = AI_PASSIVE;
                hint.ai_socktype = SOCK_STREAM;
                auto err = ::getaddrinfo(nullptr, serv->port.c_str(), &hint, &result);
                if (err != 0) {
                    return false;
                }
                serv->info = result;
                for (auto p = result; p; p = p->ai_next) {
                    auto sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                    if (sock < 0 || sock == -1) {
                        continue;
                    }
                    u_long flag = 0;
                    if (serv->ipver == 6) {
                        err = ::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag));
                        if (err < 0) {
                            ::closesocket(sock);
                            continue;
                        }
                    }
                    if (any(serv->flag & ServFlag::reuse_addr)) {
                        err = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag));
                        if (err < 0) {
                            ::closesocket(sock);
                            continue;
                        }
                    }
                    err = ::bind(sock, p->ai_addr, p->ai_addrlen);
                    if (err < 0) {
                        ::closesocket(sock);
                        continue;
                    }
                    err = ::listen(sock, 0);
                    if (err < 0) {
                        ::closesocket(sock);
                        continue;
                    }
                    serv->litener = sock;
                    serv->selected = p;
                    return true;
                }
                return false;
            }

            CNet* waiting_for_accept(CNet* ctx, TCPServer* serv) {
                serv->status = TCPStatus::wait_accept;
                auto sel = selecting_loop(ctx, serv->litener, false, serv->status, serv->timeout);
                if (sel <= 0) {
                    if (sel < 0) {
                        serv->status = TCPStatus::timeout;
                    }
                    return nullptr;
                }
                ::sockaddr_storage st;
                ::socklen_t len = sizeof(st);
                auto sock = ::accept(serv->litener, (::sockaddr*)&st, &len);
                if (sock < 0 || sock == -1) {
                    return nullptr;
                }
                u_long nonblock = 1;
                ::ioctlsocket(sock, FIONBIO, &nonblock);
                return create_server_conn(sock, &st, len);
            }

            bool server_setting_number(CNet* ctx, TCPServer* serv, std::int64_t key, std::int64_t value) {
                if (key == ipver) {
                    serv->ipver = value;
                }
                else if (key == reuse_address) {
                    if (value) {
                        serv->flag |= ServFlag::reuse_addr;
                    }
                    else {
                        serv->flag &= ~ServFlag::reuse_addr;
                    }
                }
                else if (key == conn_timeout_msec) {
                    serv->timeout = value;
                }
                else {
                    return false;
                }
                return true;
            }

            bool server_setting_ptr(CNet* ctx, TCPServer* serv, std::int64_t key, void* value) {
                if (key == port_number) {
                    auto port = (const char*)value;
                    serv->port = {};
                    helper::append(serv->port, port);
                }
                else {
                    return false;
                }
                return true;
            }

            std::int64_t query_server_num(CNet* ctx, TCPServer* serv, std::int64_t key) {
                if (key == protocol_type) {
                    return conn_acceptor;
                }
                else if (key == error_code) {
                    return net::errcode();
                }
                else if (key == current_status) {
                    return (std::int64_t)serv->status;
                }
                return 0;
            }

            void* query_server(CNet* ctx, TCPServer* serv, std::int64_t key) {
                if (key == protocol_name) {
                    return (void*)"tcp-server";
                }
                else if (key == wait_for_accept) {
                    return waiting_for_accept(ctx, serv);
                }
                return nullptr;
            }

            void close_server(CNet* ctx, TCPServer* serv) {
                ::closesocket(serv->litener);
                ::freeaddrinfo(serv->info);
                serv->litener = -1;
                serv->info = nullptr;
                serv->selected = nullptr;
            }

            CNet* STDCALL create_server() {
                ProtocolSuite<TCPServer> server_proto{
                    .initialize = tcp_server_init,
                    .uninitialize = close_server,
                    .settings_number = server_setting_number,
                    .settings_ptr = server_setting_ptr,
                    .query_number = query_server_num,
                    .query_ptr = query_server,
                    .deleter = [](TCPServer* t) { delete t; },
                };
                return cnet::create_cnet(CNetFlag::final_link | CNetFlag::init_before_io, new TCPServer{}, server_proto);
            }
        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
