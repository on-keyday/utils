/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/sockdll.h>
#include <dnet/addrinfo.h>
#include <utf/convert.h>
#include <number/array.h>
#include <dnet/dll/glheap.h>
#include <helper/defer.h>
#include <dnet/errcode.h>
#include <helper/strutil.h>
#include <dnet/bytelen.h>
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
            socdl.FreeAddrInfoExW_(raw_paddrinfo(p));
        }
        using Host = number::Array<wchar_t, 255, true>;
        using Port = number::Array<wchar_t, 20, true>;
        constexpr auto cancel_ok = 0;
#else
        using raw_paddrinfo = addrinfo*;
        void free_addr(void* p) {
            socdl.freeaddrinfo_(raw_paddrinfo(p));
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

        /*
        const void* AddrInfo::sockaddr(SockAddr& info) const {
            if (!select) {
                return nullptr;
            }
            auto ptr = raw_paddrinfo(select);
            info.af = ptr->ai_family;
            info.type = ptr->ai_socktype;
            info.proto = ptr->ai_protocol;
            info.flag = ptr->ai_flags;
            info.addr = reinterpret_cast<const raw_address*>(ptr->ai_addr);
            info.addrlen = ptr->ai_addrlen;
            return ptr->ai_addr;
        }

        NetAddrPort AddrInfo::netaddr() const {
            if (!select) {
                return {};
            }
            auto ptr = raw_paddrinfo(select);
            return sockaddr_to_NetAddrPort(ptr->ai_addr, ptr->ai_addrlen);
        }*/

        bool string_from_sockaddr_impl(int af, const void* addr, size_t addrlen, void* text, size_t len, int* err, int* port) {
            const char* ptr = nullptr;
            char* dst = static_cast<char*>(text);
            memset(text, 0, len);
            auto set_err = [&](auto code) {
                if (err) {
                    *err = code;
                }
            };
            auto pb = helper::CharVecPushbacker((byte*)text, len);
            if (af == AF_INET) {
                if (addrlen < sizeof(sockaddr_in)) {
                    set_err(invalid_argument);
                    return false;
                }
                auto p = static_cast<const sockaddr_in*>(addr);
                if (port) {
                    *port = netorder(p->sin_port);
                }
                ipaddr::ipv4_to_string(pb, (byte*)p);
            }
            else if (af == AF_INET6) {
                if (addrlen < sizeof(sockaddr_in6)) {
                    set_err(invalid_argument);
                    return false;
                }
                auto p = static_cast<const sockaddr_in6*>(addr);
                if (port) {
                    *port = netorder(p->sin6_port);
                }
                auto ipv6 = (byte*)&p->sin6_addr;
                if (ipaddr::is_ipv4_mapped(ipv6)) {
                    ipaddr::ipv4_to_string(pb, ipv6 + 12);
                }
                else {
                    ipaddr::ipv6_to_string(pb, ipv6);
                }
            }
            else {
                set_err(invalid_argument);
                return false;
            }
            if (pb.overflow) {
                set_err(invalid_addr);
                return false;
            }
            /*
            // NOTE: remove ipv6 prefix for ipv4 mapped addresss
            if (helper::contains(ptr, ".") && helper::starts_with(ptr, "::ffff:")) {
                constexpr auto start_offset = 7;
                auto tlen = strlen(ptr);
                // thought experiment
                // ::ffff:127.0.0.1----
                // tlen = 16 len = 20 (need to remain) = 9 from offset 7
                // tlen - 7 = 9 = (need to remain)
                // after memmove
                // 127.0.0.17.0.0.1----
                // (need to clear) = 7 from offset 9
                // after memset
                // 127.0.0.1-----------
                memmove(dst, dst + start_offset, len - start_offset);
                memset(dst + tlen - start_offset, 0, start_offset);
            }*/
            return true;
        }

        dnet_dll_implement(bool) string_from_sockaddr(const void* addr, size_t addrlen, void* text, size_t len, int* port, int* err) {
            if (!addr) {
                return false;
            }
            auto sa = static_cast<const sockaddr*>(addr);
            return string_from_sockaddr_impl(sa->sa_family, addr, addrlen, text, len, err, port);
        }

        bool AddrInfo::string(void* text, size_t len, int* port, int* err) const {
            if (!select) {
                return false;
            }
            auto info = static_cast<raw_paddrinfo>(select);
            return string_from_sockaddr_impl(info->ai_family, info->ai_addr, info->ai_addrlen, text, len, err, port);
        }

        struct WaitObject {
            raw_paddrinfo info;
#ifdef _WIN32
            OVERLAPPED ol;
            HANDLE cancel;
            bool done_immediate = false;
            void plt_clean() {
                socdl.GetAddrInfoExCancel_(&cancel);
            }
            int plt_cancel() {
                return socdl.GetAddrInfoExCancel_(&cancel);
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
                    auto res = kerlib.gai_cancel_(&cb);
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
                return kerlib.gai_cancel_(&cb);
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
                auto res = socdl.GetAddrInfoExOverlappedResult_(&obj->ol);
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
            auto res = socdl.GetAddrInfoExW_(host_str, port_str, 0, nullptr, &hint, &obj->info, &timeout, &obj->ol, nullptr, &obj->cancel);
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
            if (!init_sockdl()) {
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
            return resolve_address({}, port, attr);
        }

        dnet_dll_export(WaitAddrInfo) get_self_host_address(view::rvec port, SockAttr attr) {
            if (!init_sockdl()) {
                return {nullptr, libload_failed};
            }
            char host[256]{0};
            auto err = socdl.gethostname_(host, sizeof(host));
            if (err != 0) {
                return {nullptr, (int)get_error()};
            }
            return resolve_address(host, port, attr);
        }
    }  // namespace dnet
}  // namespace utils
