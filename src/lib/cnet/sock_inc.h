/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// sock_inc - socket common include
#pragma once
#include "../../include/platform/windows/dllexport_header.h"
#include "../../include/net/core/init_net.h"
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
typedef addrinfo ADDRINFOEXW;
#endif

namespace utils {
    namespace cnet {
        namespace tcp {
            CNet* create_server_conn(::SOCKET sock, ::sockaddr_storage* info, size_t len);

            int selecting_loop(CNet* ctx, ::SOCKET proto, bool send, TCPStatus& status, std::int64_t timeout);
        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
