/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/cnet/tcp.h"
#include "sock_inc.h"

namespace utils {
    namespace cnet {
        namespace tcp {
            struct TCPServer {
                ::SOCKET litener;
                TCPStatus status;
            };

            bool tcp_server_init(CNet* ctx, TCPServer* serv) {
            }

            CNet* waiting_for_accept(CNet* ctx, TCPServer* serv) {
                if (!selecting_loop(ctx, serv->litener, false, serv->status)) {
                    return nullptr;
                }
                ::sockaddr_storage st;
                ::socklen_t len = 0;
                auto sock = ::accept(serv->litener, (::sockaddr*)&st, &len);
                if (sock < 0 || sock == -1) {
                    return nullptr;
                }
            }

            std::int64_t query_number(CNet* ctx, TCPServer* serv, std::int64_t key) {
                if (key == protocol_type) {
                    return conn_acceptor;
                }
                else if (key == error_code) {
                    return net::errcode();
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

            CNet* STDCALL create_server() {
                ProtocolSuite<TCPServer> server_proto{
                    .initialize = tcp_server_init,
                    .query_ptr = query_server,
                    .deleter = [](TCPServer* t) { delete t; },
                };
            }
        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
