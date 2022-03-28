/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/cnet/tcp.h"
#include "../../include/net/core/init_net.h"
#include <cstdint>
#include "../../include/number/array.h"
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

            struct OsDns {
                number::Array<254, char, true> host{0};
                number::Array<10, char, true> port{0};
            };

            bool open_socket(CNet* ctx, OsTCPSocket* sock) {
                if (!net::network().initialized()) {
                    return false;
                }
                auto dns = cnet::get_lowlevel_protocol(ctx);
                size_t w = 0;
                request(dns, &w);
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
                ProtocolSuite<OsDns> dns_proto{
                    .deleter = [](OsDns* p) { delete p; },
                };
                auto ctx = create_cnet(CNetFlag::once_set_no_delete_link | CNetFlag::init_before_io, new OsTCPSocket{}, tcp_proto);
                auto dns = create_cnet(CNetFlag::final_link, new OsDns{}, dns_proto);
                set_lowlevel_protocol(ctx, dns);
                return ctx;
            }

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils