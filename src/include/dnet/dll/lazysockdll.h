/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "lazy.h"
#include "../plthead.h"

namespace utils {
    namespace dnet::lazy {
#ifdef _WIN32
        extern DLL ws2_32;
        extern DLL kernel32;

#define LAZY(func) inline Func<decltype(func)> func##_(ws2_32, #func);
#else
        extern DLL libanl;
#define LAZY(func) inline decltype(func)* const func##_ = func;
#endif
        LAZY(socket)
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

#ifdef _WIN32
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
        LAZY(freeaddrinfo)
        LAZY(close)
        LAZY(ioctl)
        LAZY(epoll_create1)
        LAZY(epoll_ctl)
        LAZY(epoll_pwait)
#undef LAZY
#define LAZY(func) inline Func<decltype(func)> func##_(libanl, #func);
        LAZY(getaddrinfo_a)
        LAZY(gai_error)
        LAZY(gai_cancel)
#endif

#undef LAZY
    }  // namespace dnet::lazy
}  // namespace utils
