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
#ifdef _WIN32
#ifdef __MINGW32__
#define _WIN32_WINNT 0x0501
#endif
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
#define closesocket close
#define ioctlsocket ioctl
#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD
#define SD_BOTH SHUT_RDWR
#define WSAWOULDBLOCK EWOULDBLOCK
typedef int SOCKET;
#endif

namespace utils {
    namespace cnet {
        namespace tcp {
            struct OsTCPSocket {
                ::OVERLAPPED ol;
                ::SOCKET sock;
                number::Array<254, char, true> host{0};
                number::Array<10, char, true> port{0};
                ::addrinfo* info = nullptr;
            };

            bool open_socket(CNet* ctx, OsTCPSocket* sock) {
                if (!net::network().initialized()) {
                    return false;
                }
                ADDRINFOEXA info{0}, *result;
                ::timeval timeout{0};
                sock->ol.hEvent = ::CreateEventA(nullptr, true, false, nullptr);

                ::GetAddrInfoExA(
                    sock->host.c_str(), sock->port.c_str(),
                    NS_DNS, nullptr, &info, &result, &timeout, &sock->ol, nullptr, nullptr);
            }

            void close_socket(CNet* ctx, OsTCPSocket* sock) {}

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
                auto ctx = create_cnet(CNetFlag::final_link | CNetFlag::init_before_io, new OsTCPSocket{}, tcp_proto);
                return ctx;
            }

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils