/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// plthead - platform depended headers
#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>

#include <afunix.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#endif

namespace utils {
    namespace dnet {
#ifdef _WIN32
        using raw_addrinfo = ADDRINFOEXW;
#else
        using raw_addrinfo = addrinfo;
#endif
    }  // namespace dnet
}  // namespace utils
