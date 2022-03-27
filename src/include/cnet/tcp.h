/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp cnet interface
#pragma once
#include <cstdint>
#include "cnet.h"

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

            bool open_socket(CNet* ctx, OsTCPSocket*);
            void close_socket(CNet* ctx, OsTCPSocket* sock);

            bool write_socket(CNet* ctx, OsTCPSocket* sock, Buffer<const char>* buf);
            bool read_socket(CNet* ctx, OsTCPSocket* user, Buffer<char>* buf);

            ProtocolSuite<OsTCPSocket> tcp_proto{
                .initialize = open_socket,
                .write = write_socket,
                .read = read_socket,
                .uninitialize = close_socket,
                .deleter = [](OsTCPSocket* sock) { delete sock; },
            };
        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
