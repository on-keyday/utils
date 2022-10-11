/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dynload.h"
#include "../plthead.h"
namespace utils {
    namespace dnet {

        struct Kernel {
           private:
#define L(func) LOADER_BASE(func, func, lib, Kernel, false)
#ifdef _WIN32
            alib<3> lib;
            L(CreateIoCompletionPort)
            L(CancelIoEx)
            L(GetQueuedCompletionStatusEx)
#else
            alib<3> lib;
            L(getaddrinfo_a)  // libanl
            L(gai_error)
            L(gai_cancel)
           public:
            const char* libanl = "libanl.so";
#endif
           private:
            static Kernel instance;
            constexpr Kernel() {}
            friend Kernel& get_ker32_instacne();

           public:
            bool load();
#undef L
        };

        extern Kernel& kerlib;

        struct SocketDll {
           private:
#ifdef _WIN32
            static constexpr auto libcount = 30;
#else
            static constexpr auto libcount = 21;
#endif
            alib<libcount> lib;
#define L(func) LOADER_BASE(func, func, lib, SocketDll, false)
            L(socket)
            L(send)
            L(recv)
            L(sendto)
            L(recvfrom)
            L(setsockopt)
            L(getsockopt)
            L(shutdown)
            L(connect)
            L(bind)
            L(listen)
            L(accept)
            L(select)
            L(inet_ntop)
            L(gethostname)
#ifdef _WIN32
            L(closesocket)
            L(ioctlsocket)
            L(WSAStartup)
            L(WSASocketW)
            L(WSARecv)
            L(WSARecvFrom)
            L(WSASend)
            L(WSASendTo)
            L(WSACleanup)
            L(WSAIoctl)
            LPFN_ACCEPTEX AcceptEx_;
            LPFN_CONNECTEX ConnectEx_;
            L(GetAddrInfoExW)
            L(FreeAddrInfoExW)
            L(GetAddrInfoExCancel)
            L(GetAddrInfoExOverlappedResult)
            L(__WSAFDIsSet)
#else
            L(freeaddrinfo)
            L(close)
            L(ioctl)
            L(epoll_create1)
            L(epoll_ctl)
            L(epoll_pwait)
#endif
           private:
            static SocketDll instance;
            constexpr SocketDll() {}
            friend SocketDll& get_sock_instance();

           public:
            bool load();
            ~SocketDll();
#undef L
        };
        extern SocketDll& socdl;
        bool init_sockdl();
#undef LOADER_BASE
    }  // namespace dnet
}  // namespace utils
