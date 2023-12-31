/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/detect.h>
#include "lazy.h"
#include "../../plthead.h"

namespace utils {
    namespace fnet::lazy {
#ifdef UTILS_PLATFORM_WINDOWS
        extern DLL ws2_32;
        extern DLL kernel32;

        inline bool load_socket() {
            return ws2_32.load();
        }

        inline bool load_addrinfo() {
            return ws2_32.load();
        }

#define LAZY(func) inline Func<decltype(func)> func##_{ws2_32, #func};
#else

#define LAZY(func) inline decltype(func)* const func##_ = func;
#endif
        LAZY(send)
        LAZY(recv)
        LAZY(sendto)
        LAZY(recvfrom)
        LAZY(setsockopt)
        LAZY(getsockopt)
        LAZY(shutdown)
        LAZY(connect)
        LAZY(bind)
        LAZY(listen)
        LAZY(accept)
        LAZY(select)
        LAZY(gethostname)
        LAZY(getsockname)
        LAZY(getpeername)

#ifdef UTILS_PLATFORM_WINDOWS
        LAZY(closesocket)
        LAZY(ioctlsocket)
        LAZY(WSAStartup)
        LAZY(WSASocketW)
        LAZY(WSARecv)
        LAZY(WSARecvFrom)
        LAZY(WSASend)
        LAZY(WSASendTo)
        LAZY(WSAIoctl)
        inline Func<std::remove_pointer_t<LPFN_ACCEPTEX>> AcceptEx_(ws2_32, "AcceptEx");
        inline Func<std::remove_pointer_t<LPFN_CONNECTEX>> ConnectEx_(ws2_32, "ConnectEx");
        LAZY(GetAddrInfoExW)
        LAZY(FreeAddrInfoExW)
        LAZY(GetAddrInfoExCancel)
        LAZY(GetAddrInfoExOverlappedResult)
        LAZY(__WSAFDIsSet)
        LAZY(WSAEnumProtocolsW)
#undef LAZY
#define LAZY(func) inline Func<decltype(func)> func##_(kernel32, #func);

        LAZY(CreateIoCompletionPort)
        LAZY(CancelIoEx)
        LAZY(GetQueuedCompletionStatusEx)
        LAZY(SetFileCompletionNotificationModes)
#else
        LAZY(socket)
        LAZY(freeaddrinfo)
        LAZY(close)
        LAZY(ioctl)
        LAZY(getaddrinfo)
#ifdef UTILS_PLATFORM_LINUX
        LAZY(epoll_create1)
        LAZY(epoll_ctl)
        LAZY(epoll_pwait)
#endif
#undef LAZY

        extern DLL libanl;
        constexpr bool load_socket() {
            return true;
        }

        inline bool load_addrinfo() {
            return libanl.load();
        }

#define LAZY(func) inline Func<decltype(func)> func##_(libanl, #func);

#ifdef FNET_HAS_ASYNC_GETADDRINFO
        LAZY(getaddrinfo_a)
        LAZY(gai_error)
        LAZY(gai_cancel)
        LAZY(gai_strerror)
#endif
#endif

#undef LAZY
    }  // namespace fnet::lazy
}  // namespace utils
