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
#include <helper/view.h>
#include <dnet/dll/glheap.h>
#include <quic/mem/raii.h>
#include <dnet/errcode.h>
#include <helper/strutil.h>
#include <endian/endian.h>
#include <cstring>
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
        using Host = number::Array<255, wchar_t, true>;
        using Port = number::Array<20, wchar_t, true>;
        constexpr auto cancel_ok = 0;
#else
        using raw_paddrinfo = addrinfo*;
        void free_addr(void* p) {
            socdl.freeaddrinfo_(raw_paddrinfo(p));
        }
        using Host = number::Array<255, char, true>;
        using Port = number::Array<20, char, true>;
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

        const void* AddrInfo::sockaddr(SockAddr& info) const {
            if (!select) {
                return nullptr;
            }
            auto ptr = raw_paddrinfo(select);
            info.af = ptr->ai_family;
            info.type = ptr->ai_socktype;
            info.proto = ptr->ai_protocol;
            info.flag = ptr->ai_flags;
            info.addr = ptr->ai_addr;
            info.addrlen = ptr->ai_addrlen;
            return ptr->ai_addr;
        }

        bool string_from_sockaddr_impl(int af, const void* addr, size_t addrlen, void* text, size_t len, int* err, int* port) {
            const char* ptr = nullptr;
            char* dst = static_cast<char*>(text);
            memset(text, 0, len);
            if (af == AF_INET) {
                if (addrlen < sizeof(sockaddr_in)) {
                    set_error(invalid_argument);
                }
                else {
                    auto p = static_cast<const sockaddr_in*>(addr);
                    if (port) {
                        *port = endian::from_network(&p->sin_port);
                    }
                    ptr = socdl.inet_ntop_(af, &p->sin_addr, dst, len);
                }
            }
            else if (af == AF_INET6) {
                if (addrlen < sizeof(sockaddr_in6)) {
                    set_error(invalid_argument);
                }
                else {
                    auto p = static_cast<const sockaddr_in6*>(addr);
                    if (port) {
                        *port = endian::from_network(&p->sin6_port);
                    }
                    ptr = socdl.inet_ntop_(af, &p->sin6_addr, dst, len);
                }
            }
            else {
                set_error(invalid_argument);
            }
            if (!ptr) {
                if (err) {
                    *err = get_error();
                }
                return false;
            }
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
            }
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
            bool done_immdedeate = false;
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
                if (!obj->done_immdedeate && res != 0) {
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

        static WaitObject* platform_resolve_address(const SockAddr& addr, int& err, Host* host, Port* port) {
            ADDRINFOEXW hint{};
            hint.ai_family = addr.af;
            hint.ai_socktype = addr.type;
            hint.ai_protocol = addr.proto;
            hint.ai_flags = addr.flag;
            timeval timeout;
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            auto obj = new_from_global_heap<WaitObject>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject)));
            if (!obj) {
                err = no_resource;
                return nullptr;
            }
            quic::mem::RAII r{obj, [](auto obj) {
                                  delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject)));
                              }};
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
            obj->done_immdedeate = res == 0;
            r.t = nullptr;  // release
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

        static WaitObject* platform_resolve_address(const SockAddr& addr, int& err, Host* host, Port* port) {
            auto obj = new_from_global_heap<WaitObject>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject)));
            if (!obj) {
                err = no_resource;
                return nullptr;
            }
            quic::mem::RAII r{obj, [](auto obj) {
                                  delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject)));
                              }};
            obj->hint.ai_family = addr.af;
            obj->hint.ai_socktype = addr.type;
            obj->hint.ai_protocol = addr.proto;
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
            r.t = nullptr;
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
                delete_with_global_heap(obj, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(WaitObject)));
            }
        }

        dnet_dll_implement(WaitAddrInfo) resolve_address(const SockAddr& addr, const char* port) {
            if (!init_sockdl()) {
                return {
                    nullptr,
                    libload_failed,
                };
            }
            Host host{};
            Port port_{};
            if (addr.hostname) {
                utf::convert(helper::SizedView(addr.hostname, addr.namelen), host);
            }
            if (port) {
                utf::convert(port, port_);
            }
            int err = 0;
            auto obj = platform_resolve_address(addr, err, addr.hostname ? &host : nullptr, port ? &port_ : nullptr);
            if (!obj) {
                return {nullptr, err};
            }
            return {obj, 0};
        }

        dnet_dll_implement(WaitAddrInfo) get_self_server_address(const SockAddr& addr, const char* port) {
            auto hint = addr;
            hint.hostname = nullptr;
            hint.namelen = 0;
            hint.flag |= AI_PASSIVE;
            return resolve_address(hint, port);
        }

        dnet_dll_export(WaitAddrInfo) get_self_host_address(const SockAddr& addr, const char* port) {
            if (!init_sockdl()) {
                return {nullptr, libload_failed};
            }
            char host[256]{0};
            auto err = socdl.gethostname_(host, sizeof(host));
            if (err != 0) {
                return {nullptr, (int)get_error()};
            }
            auto hint = addr;
            hint.hostname = host;
            hint.namelen = utils::strlen(host);
            return resolve_address(hint, port);
        }
    }  // namespace dnet
}  // namespace utils
