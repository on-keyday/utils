/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// udp - io udp wrapper
#pragma once
#include <quic/io/io.h>
#include <quic/io/udp.h>
#include <quic/internal/alloc.h>
#include <quic/common/variable_int.h>

// platform dependency
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace utils {
    namespace quic {
        namespace io {
            namespace udp {
                struct Conn {
                    allocate::Alloc* a;
                };

                const char* topchar(const byte* d) {
                    return reinterpret_cast<const char*>(d);
                }

                io::Result write_to(Conn* c, const Target* t, const byte* d, tsize l, tsize* w) {
                    if (t->key != UDP_IPv4 && t->key != UDP_IPv6) {
                        return {io::Status::unsupported, invalid, true};
                    }
                    if (t->target == invalid || t->target == global) {
                        return {io::Status::invalid_arg, invalid, true};
                    }
                    if (t->key == UDP_IPv4) {
                        ::sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = varint::swap_if(t->ip.port);
                        varint::Swap<uint> a;
                        a.swp[0] = t->ip.address[0];
                        a.swp[1] = t->ip.address[1];
                        a.swp[2] = t->ip.address[2];
                        a.swp[3] = t->ip.address[3];
                        addr.sin_addr.s_addr = a.t;
                    }
                    else {
                        ::sockaddr_in6 addr;
                        addr.sin6_family = AF_INET6;
                        addr.sin6_port = varint::swap_if(t->ip.port);
                        bytes::copy(addr.sin6_addr.s6_addr, t->ip.address, 16);
                    }
                }

                io::IO Protocol(allocate::Alloc* a) {
                }
            }  // namespace udp
        }      // namespace io
    }          // namespace quic
}  // namespace utils
