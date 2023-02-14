/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/lazy/sockdll.h>
#include <dnet/addrinfo.h>
#include <unicode/utf/convert.h>
#include <number/array.h>
#include <dnet/dll/glheap.h>
#include <helper/defer.h>
#include <dnet/errcode.h>
#include <helper/strutil.h>
#include <bit>
#include <cstring>
#include <dnet/dll/errno.h>
#include <net_util/ipaddr.h>
#include <dnet/dll/asyncbase.h>
#include <view/charvec.h>
#ifndef _WIN32
#include <signal.h>
#include <thread>
#endif

namespace utils {
    namespace dnet {
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
            int plt_cancel() {
                return lazy::GetAddrInfoExCancel_(&cancel);
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

            int plt_cancel() {
                return lazy::gai_cancel_(&cb);
            }
#endif
            ~WaitObject() {
                plt_clean();
                if (info) {
                    free_addr(info);
                }
            }
        };

        bool WaitAddrInfo::failed(int* e) const {
            if (!err) {
                return false;
            }
            if (e) {
                *e = err;
            }
            return true;
        }
#ifdef _WIN32
        static bool platform_wait(WaitObject* obj, int& err, std::uint32_t time) {
            int sig = 0;
            if (!obj->info) {
                sig = WaitForSingleObject(obj->ol.hEvent, time);
            }
            if (sig == WAIT_OBJECT_0) {
                auto res = lazy::GetAddrInfoExOverlappedResult_(&obj->ol);
                ResetEvent(obj->ol.hEvent);
                CloseHandle(obj->ol.hEvent);
                if (!obj->done_immediate && res != 0) {
                    err = res;
                    return false;
                }
                return true;
            }
            if (sig == WAIT_TIMEOUT) {
                return false;
            }
            err = get_error();
            return false;
        }

        static WaitObject* platform_resolve_address(const SockAttr& addr, int& err, Host* host, Port* port) {
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
                err = no_resource;
                return nullptr;
            }
            auto r = helper::defer([&] {
                delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            });
            auto event = CreateEventW(nullptr, true, false, nullptr);
            if (!event) {
                err = no_resource;
                return nullptr;
            }
            obj->ol.hEvent = event;
            auto host_str = host ? host->c_str() : nullptr;
            auto port_str = port ? port->c_str() : nullptr;
            auto res = lazy::GetAddrInfoExW_(host_str, port_str, 0, nullptr, &hint, &obj->info, &timeout, &obj->ol, nullptr, &obj->cancel);
            if (res != 0 && res != WSA_IO_PENDING) {
                err = get_error();
                return nullptr;
            }
            obj->done_immediate = res == 0;
            r.cancel();
            return obj;
        }
#else
        static bool platform_wait(WaitObject* obj, int& err, std::uint32_t mili) {
            using namespace std::chrono_literals;
            std::uint32_t msec = 0;
            constexpr auto ms = 9600us;
            while (msec < mili) {
                auto res = kerlib.gai_error_(&obj->cb);
                if (res != EAI_INPROGRESS) {
                    err = res;
                    obj->info = obj->cb.ar_result;
                    obj->cb.ar_result = nullptr;
                    return res == 0;
                }
                std::this_thread::sleep_for(ms);
                msec++;
            }
            set_error(ETIMEDOUT);
            return false;
        }

        static WaitObject* platform_resolve_address(const SockAttr& addr, int& err, Host* host, Port* port) {
            auto obj = new_from_global_heap<WaitObject>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            if (!obj) {
                err = no_resource;
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
            auto res = kerlib.getaddrinfo_a_(GAI_NOWAIT, list, 1, &obj->sig);
            if (res != 0) {
                err = res;
                return nullptr;
            }
            r.cancel();
            return obj;
        }
#endif

        bool WaitAddrInfo::wait(AddrInfo& info, std::uint32_t time) {
            if (!opt) {
                return false;
            }
            auto obj = static_cast<WaitObject*>(opt);
            if (platform_wait(obj, err, time)) {
                info = {obj->info};
                obj->info = nullptr;
                return true;
            }
            return false;
        }

        bool WaitAddrInfo::cancel() {
            if (!opt) {
                err = no_resource;
                return false;
            }
            auto res = static_cast<WaitObject*>(opt)->plt_cancel();
            if (res != cancel_ok) {
                err = res;
                return false;
            }
            return true;
        }

        WaitAddrInfo::~WaitAddrInfo() {
            if (opt) {
                auto obj = static_cast<WaitObject*>(opt);
                delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject), alignof(WaitObject)));
            }
        }

        dnet_dll_implement(WaitAddrInfo) resolve_address(view::rvec hostname, view::rvec port, SockAttr attr) {
            if (!lazy::load_socket()) {
                return {
                    nullptr,
                    libload_failed,
                };
            }
            Host host{};
            Port port_{};
            if (!hostname.null()) {
                utf::convert(view::rvec(hostname), host);
            }
            if (!port.null()) {
                utf::convert(port, port_);
            }
            int err = 0;
            auto obj = platform_resolve_address(attr, err, !hostname.null() ? &host : nullptr, !port.null() ? &port_ : nullptr);
            if (!obj) {
                return {nullptr, err};
            }
            return {obj, 0};
        }

        dnet_dll_implement(WaitAddrInfo) get_self_server_address(view::rvec port, SockAttr attr) {
            attr.flag |= AI_PASSIVE;
            return resolve_address({}, port, attr);
        }

        dnet_dll_export(WaitAddrInfo) get_self_host_address(view::rvec port, SockAttr attr) {
            if (!lazy::load_socket()) {
                return {nullptr, libload_failed};
            }
            char host[256]{0};
            auto err = lazy::gethostname_(host, sizeof(host));
            if (err != 0) {
                return {nullptr, (int)get_error()};
            }
            return resolve_address(host, port, attr);
        }

        dnet_dll_implement(SockAttr) sockattr_tcp(int ipver) {
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

        dnet_dll_implement(SockAttr) sockattr_udp(int ipver) {
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
    }  // namespace dnet
}  // namespace utils
