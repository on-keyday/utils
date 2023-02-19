/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/addrinfo.h>
#include <unicode/utf/convert.h>
#include <number/array.h>
#include <fnet/dll/glheap.h>
#include <helper/defer.h>
#include <helper/strutil.h>
#include <bit>
#include <cstring>
#include <fnet/dll/errno.h>
#include <net_util/ipaddr.h>
#include <fnet/dll/asyncbase.h>
#include <view/charvec.h>
#ifndef _WIN32
#include <signal.h>
#include <thread>
#endif

namespace utils {
    namespace fnet {
#ifdef _WIN32
        using raw_paddrinfo = PADDRINFOEXW;
        void free_addr(void* p) {
            lazy::FreeAddrInfoExW_(raw_paddrinfo(p));
        }
        using Host = number::Array<wchar_t, 255, true>;
        using Port = number::Array<wchar_t, 20, true>;
        constexpr auto cancel_ok = 0;
#else
        using raw_paddrinfo = addrinfo*;
        void free_addr(void* p) {
            lazy::freeaddrinfo_(raw_paddrinfo(p));
        }
        using Host = number::Array<char, 255, true>;
        using Port = number::Array<char, 20, true>;
        constexpr auto cancel_ok = EAI_CANCELED;
#endif

        AddrInfo::~AddrInfo() {
            free_addr(root);
        }

        void* AddrInfo::next() {
            if (!select) {
                select = root;
                return select;
            }
            auto ptr = raw_paddrinfo(select);
            select = ptr->ai_next;
            return select;
        }

        SockAddr AddrInfo::sockaddr() const {
            SockAddr addr;
            auto ptr = raw_paddrinfo(select);
            addr.addr = sockaddr_to_NetAddrPort(ptr->ai_addr, ptr->ai_addrlen);
            addr.attr.address_family = ptr->ai_family;
            addr.attr.socket_type = ptr->ai_socktype;
            addr.attr.protocol = ptr->ai_protocol;
            addr.attr.flag = ptr->ai_flags;
            return addr;
        }

        struct WaitObject {
            raw_paddrinfo info;
#ifdef _WIN32
            OVERLAPPED ol;
            HANDLE cancel;
            bool done_immediate = false;
            void plt_clean() {
                lazy::GetAddrInfoExCancel_(&cancel);
            }

#else
            addrinfo hint;
            gaicb cb;
            Host host;
            Port port;
            sigevent sig;
            void plt_clean() {
                int one = 0;
                while (one < 100) {
                    auto res = lazy::gai_cancel_(&cb);
                    if (res == EAI_ALLDONE) {
                        if (cb.ar_result) {
                            free_addr(cb.ar_result);
                        }
                    }
                    else if (res == EAI_NOTCANCELED) {
                        one++;
                        continue;
                    }
                    break;
                }
            }

#endif
            ~WaitObject() {
                plt_clean();
                if (info) {
                    free_addr(info);
                }
            }
        };

#ifdef _WIN32

        static bool platform_cancel(WaitObject* obj, error::Error& err) {
            auto res = lazy::GetAddrInfoExCancel_(&obj->cancel);
            if (res != 0) {
                err = error::Error(res, error::ErrorCategory::syserr);
                return false;
            }
            return true;
        }

        static bool platform_wait(WaitObject* obj, error::Error& err, std::uint32_t time) {
            int sig = WAIT_OBJECT_0;
            if (!obj->info) {
                sig = WaitForSingleObject(obj->ol.hEvent, time);
            }
            if (sig == WAIT_OBJECT_0) {
                auto res = lazy::GetAddrInfoExOverlappedResult_(&obj->ol);
                ResetEvent(obj->ol.hEvent);
                CloseHandle(obj->ol.hEvent);
                if (!obj->done_immediate && res != 0) {
                    err = error::Error(res, error::ErrorCategory::syserr);
                    return false;
                }
                return true;
            }
            if (sig == WAIT_TIMEOUT) {
                err = error::block;
                return false;
            }
            err = error::Error(get_error(), error::ErrorCategory::syserr);
            return false;
        }

        static WaitObject* platform_resolve_address(const SockAttr& addr, error::Error& err, Host* host, Port* port) {
            ADDRINFOEXW hint{};
            hint.ai_family = addr.address_family;
            hint.ai_socktype = addr.socket_type;
            hint.ai_protocol = addr.protocol;
            hint.ai_flags = addr.flag;
            timeval timeout;
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            auto obj = new_from_global_heap<WaitObject>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            if (!obj) {
                err = error::memory_exhausted;
                return nullptr;
            }
            auto r = helper::defer([&] {
                delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            });
            auto event = CreateEventW(nullptr, true, false, nullptr);
            if (!event) {
                err = error::Error("CreateEventW failed");
                return nullptr;
            }
            obj->ol.hEvent = event;
            auto host_str = host ? host->c_str() : nullptr;
            auto port_str = port ? port->c_str() : nullptr;
            auto res = lazy::GetAddrInfoExW_(host_str, port_str, 0, nullptr, &hint, &obj->info, &timeout, &obj->ol, nullptr, &obj->cancel);
            if (res != 0 && res != WSA_IO_PENDING) {
                err = error::Error(get_error(), error::ErrorCategory::syserr);
                return nullptr;
            }
            obj->done_immediate = res == 0;
            r.cancel();
            return obj;
        }
#else
        void map_error(int res, error::Error& err) {
            auto str = lazy::gai_strerror_(res);
            if (str) {
                err = error::Error(str, error::ErrorCategory::syserr);
            }
            else {
                err = error::Error("unexpected getaddrinfo error code", error::ErrorCategory::syserr);
            }
        }

        static bool platform_cancel(WaitObject* obj, error::Error& err) {
            auto res = lazy::gai_cancel_(&obj->cb);
            if (res == EAI_ALLDONE || res == EAI_CANCELED) {
                return true;
            }
            map_error(res, err);
            return false;
        }

        static bool platform_wait(WaitObject* obj, error::Error& err, std::uint32_t mili) {
            using namespace std::chrono_literals;
            std::uint32_t msec = 0;
            constexpr auto ms = 9600us;
            while (msec < mili) {
                auto res = lazy::gai_error_(&obj->cb);
                if (res == 0) {
                    obj->info = obj->cb.ar_result;
                    obj->cb.ar_result = nullptr;
                    return true;
                }
                if (res != EAI_INPROGRESS) {
                    map_error(res, err);
                }
                std::this_thread::sleep_for(ms);
                msec++;
            }
            err = error::Error(ETIMEDOUT, error::ErrorCategory::syserr);
            return false;
        }

        static WaitObject* platform_resolve_address(const SockAttr& addr, error::Error& err, Host* host, Port* port) {
            auto obj = new_from_global_heap<WaitObject>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            if (!obj) {
                err = error::memory_exhausted;
                return nullptr;
            }
            auto r = helper::defer([&] {
                delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            });
            obj->hint.ai_family = addr.address_family;
            obj->hint.ai_socktype = addr.socket_type;
            obj->hint.ai_protocol = addr.protocol;
            obj->hint.ai_flags = addr.flag;
            obj->cb.ar_request = &obj->hint;
            if (host) {
                obj->host = std::move(*host);
                obj->cb.ar_name = obj->host.c_str();
            }
            else {
                obj->cb.ar_name = nullptr;
            }
            if (port) {
                obj->port = std::move(*port);
                obj->cb.ar_service = obj->port.c_str();
            }
            else {
                obj->cb.ar_service = nullptr;
            }
            obj->cb.ar_result = nullptr;
            obj->sig.sigev_notify = SIGEV_NONE;
            gaicb* list[1] = {&obj->cb};
            auto res = lazy::getaddrinfo_a_(GAI_NOWAIT, list, 1, &obj->sig);
            if (res != 0) {
                map_error(res, err);
                return nullptr;
            }
            r.cancel();
            return obj;
        }
#endif
        constexpr auto errNotInitialized = error::Error("WaitAddrInfo is not initialized", error::ErrorCategory::validationerr);

        error::Error WaitAddrInfo::wait(AddrInfo& info, std::uint32_t time) {
            if (!opt) {
                return errNotInitialized;
            }
            auto obj = static_cast<WaitObject*>(opt);
            error::Error err;
            if (platform_wait(obj, err, time)) {
                info = {obj->info};
                obj->info = nullptr;
                return error::none;
            }
            return err;
        }

        error::Error WaitAddrInfo::cancel() {
            if (!opt) {
                return errNotInitialized;
            }
            error::Error err;
            auto obj = static_cast<WaitObject*>(opt);
            platform_cancel(obj, err);
            return err;
        }

        WaitAddrInfo::~WaitAddrInfo() {
            if (opt) {
                auto obj = static_cast<WaitObject*>(opt);
                delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            }
        }

        fnet_dll_implement(std::pair<WaitAddrInfo, error::Error>) resolve_address(view::rvec hostname, view::rvec port, SockAttr attr) {
            if (!lazy::load_addrinfo()) {
                return {WaitAddrInfo{}, error::Error("socket library not loaded", error::ErrorCategory::syserr)};
            }
            Host host{};
            Port port_{};
            if (!hostname.null()) {
                utf::convert(view::rvec(hostname), host);
            }
            if (!port.null()) {
                utf::convert(port, port_);
            }
            error::Error err;
            auto obj = platform_resolve_address(attr, err, !hostname.null() ? &host : nullptr, !port.null() ? &port_ : nullptr);
            if (!obj) {
                return {WaitAddrInfo{}, std::move(err)};
            }
            return {WaitAddrInfo{obj}, error::none};
        }

        fnet_dll_implement(std::pair<WaitAddrInfo, error::Error>) get_self_server_address(view::rvec port, SockAttr attr) {
            attr.flag |= AI_PASSIVE;
            return resolve_address({}, port, attr);
        }

        fnet_dll_export(std::pair<WaitAddrInfo, error::Error>) get_self_host_address(view::rvec port, SockAttr attr) {
            if (!lazy::load_addrinfo()) {
                return {WaitAddrInfo{}, error::Error("socket library not loaded", error::ErrorCategory::syserr)};
            }
            char host[256]{0};
            auto err = lazy::gethostname_(host, sizeof(host));
            if (err != 0) {
                return {WaitAddrInfo{}, error::Error(get_error(), error::ErrorCategory::syserr)};
            }
            return resolve_address(host, port, attr);
        }

        fnet_dll_implement(SockAttr) sockattr_tcp(int ipver) {
            switch (ipver) {
                case 4:
                    return SockAttr{
                        .address_family = AF_INET,
                        .socket_type = SOCK_STREAM,
                        .protocol = IPPROTO_TCP,
                    };
                case 6:
                    return SockAttr{
                        .address_family = AF_INET6,
                        .socket_type = SOCK_STREAM,
                        .protocol = IPPROTO_TCP,
                    };
                default:
                    return SockAttr{
                        .address_family = AF_UNSPEC,
                        .socket_type = SOCK_STREAM,
                        .protocol = IPPROTO_TCP,
                    };
            }
        }

        fnet_dll_implement(SockAttr) sockattr_udp(int ipver) {
            switch (ipver) {
                case 4:
                    return SockAttr{
                        .address_family = AF_INET,
                        .socket_type = SOCK_DGRAM,
                        .protocol = IPPROTO_UDP,
                    };
                case 6:
                    return SockAttr{
                        .address_family = AF_INET6,
                        .socket_type = SOCK_DGRAM,
                        .protocol = IPPROTO_UDP,
                    };
                default:
                    return SockAttr{
                        .address_family = AF_UNSPEC,
                        .socket_type = SOCK_DGRAM,
                        .protocol = IPPROTO_UDP,
                    };
            }
        }
    }  // namespace fnet
}  // namespace utils
