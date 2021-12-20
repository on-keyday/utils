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
            };
        }  // namespace internal

        wrap::shared_ptr<TCPConn> TCPResult::connect() {
            if (!impl) {
                return nullptr;
            }
        }

        bool TCPResult::failed() {
        }

        TCPResult::~TCPResult() {
        }

        TCPResult open(wrap::shared_ptr<Address>&& addr) {
            if (!addr) {
                return TCPResult();
            }
            auto info = reinterpret_cast<internal::addrinfo*>(addr->get_rawaddr());
            for (auto p = info; p; p = info->ai_next) {
                auto sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                ::u_long nonblock = 1;
                ::ioctlsocket(sock, FIONBIO, &nonblock);
                auto result = ::connect(sock, p->ai_addr, p->ai_addrlen);
            }
        }

    }  // namespace net
}  // namespace utils
