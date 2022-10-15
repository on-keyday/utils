/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// sock_internal - socket helper
#pragma once
#include "../io/io.h"
#include "../io/udp.h"
#include "../common/variable_int.h"
// platform dependency
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#define GET_ERROR() external::sockdll.WSAGetLastError_()
#define INCOMPLETE(code) (code == WSAEWOULDBLOCK)
#define TOO_LARGE(code) (code == WSAEMSGSIZE)
#define ASYNC(code) (code == WSA_IO_PENDING)
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define GET_ERROR() errno
#define INCOMPLETE(code) code == EWOULDBLOCK || code == EAGAIN
#define TOO_LARGE(code) code == EMSGSIZE
#define ASYNC(code) false
#define ioctlsocket ioctl
#define closesocket close
#endif

namespace utils {
    namespace quic {
        namespace io {

            template <class A>
            ::sockaddr* repaddr(A* a) {
                return reinterpret_cast< ::sockaddr*>(a);
            }

            bool sockaddr_to_target(::sockaddr_storage* storage, Target* t);

            auto target_to_sockaddr(const Target* t, auto&& callback) {
                if (t->key == udp::UDP_IPv4) {
                    ::sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    addr.sin_port = varint::swap_if(t->ip.port);
                    varint::Swap<uint> a;
                    a.swp[0] = t->ip.address[0];
                    a.swp[1] = t->ip.address[1];
                    a.swp[2] = t->ip.address[2];
                    a.swp[3] = t->ip.address[3];
                    addr.sin_addr.s_addr = a.t;
                    return callback(repaddr(&addr), sizeof(addr));
                }
                else {
                    ::sockaddr_in6 addr{};
                    addr.sin6_family = AF_INET6;
                    addr.sin6_port = varint::swap_if(t->ip.port);
                    bytes::copy(addr.sin6_addr.s6_addr, t->ip.address, 16);
                    return callback(repaddr(&addr), sizeof(addr));
                }
            }

            constexpr io::Result with_code(int code) {
                if (INCOMPLETE(code)) {
                    return {io::Status::incomplete, tsize(code)};
                }
                if (TOO_LARGE(code)) {
                    return {io::Status::too_large_data, tsize(code)};
                }
                return {io::Status::failed, tsize(code)};
            }

            inline char* topchar(byte* d) {
                return reinterpret_cast<char*>(d);
            }

            inline const char* topchar(const byte* d) {
                return reinterpret_cast<const char*>(d);
            }

        }  // namespace io
    }      // namespace quic
}  // namespace utils
