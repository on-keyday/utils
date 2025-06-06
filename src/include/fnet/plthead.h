/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// plthead - platform depended headers
#pragma once
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#include <afunix.h>
#include <mstcpip.h>
#elif defined(FUTILS_PLATFORM_WASI)
#include <stub/network.h>
#elif defined(FUTILS_PLATFORM_UNIX)
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <unistd.h>
#include <sys/un.h>
#ifdef FUTILS_PLATFORM_LINUX
#include <sys/epoll.h>
#include <linux/if.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#endif
#ifdef GAI_WAIT  // XXX(on-keyday): hack for getaddrinfo_a
#define FNET_HAS_ASYNC_GETADDRINFO
#endif
#endif
