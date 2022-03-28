/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/cnet/tcp.h"
#include "../../include/net/core/init_net.h"
#include <cstdint>
namespace utils {
    namespace cnet {
        namespace tcp {
            struct OsTCPSocket {
#ifdef _WIN32
                std::uintptr_t sock;
#else
                int sock;
#endif
            };

            bool open_socket(CNet* ctx, OsTCPSocket* sock) {
                if (!net::network().initialized()) {
                    return false;
                }
            }
            void close_socket(CNet* ctx, OsTCPSocket* sock);

            bool write_socket(CNet* ctx, OsTCPSocket* sock, Buffer<const char>* buf);
            bool read_socket(CNet* ctx, OsTCPSocket* user, Buffer<char>* buf);

            inline CNet* create_client() {
                ProtocolSuite<OsTCPSocket> tcp_proto{
                    .initialize = open_socket,
                    .write = write_socket,
                    .read = read_socket,
                    .uninitialize = close_socket,
                    .deleter = [](OsTCPSocket* sock) { delete sock; },
                };
                return create_cnet(CNetFlag::final_link | CNetFlag::init_before_io, new OsTCPSocket{}, tcp_proto);
            }

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils