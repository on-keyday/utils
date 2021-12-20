/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// platform - platform depended headers
#pragma once

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

#include <string>
#include <cstring>
#include <stdexcept>
#include <ctime>

#if !defined(_WIN32) || (defined(__GNUC__) && !defined(__clang__))
#define memcpy_s(dst, dsz, src, ssz) memcpy(dst, src, ssz)
#define strcpy_s(dst, dsz, src) strcpy(dst, src)
#endif

#ifndef _WIN32
#define Sleep(time) usleep(time)
#endif

#ifndef USE_OPENSSL
#define USE_OPENSSL 1
#endif

#if USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#endif

namespace utils {
    namespace net {
        namespace internal {
#ifdef _WIN32
            using addrinfo = ::addrinfoexW;

#else
            using addrinfo = ::addrinfo;

#endif
            void freeaddrinfo(addrinfo* info);
        }  // namespace internal

    }  // namespace net
}  // namespace utils
