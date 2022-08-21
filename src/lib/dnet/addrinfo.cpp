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
#else
        using raw_paddrinfo = addrinfo*;
        void free_addr(void* p) {
            socdl.freeaddrinfo_(raw_paddrinfo(p));
        }
        using Host = number::Array<255, char, true>;
        using Port = number::Array<20, char, true>;
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

        struct WaitObject {
            raw_paddrinfo info;
#ifdef _WIN32
            OVERLAPPED ol;
            HANDLE cancel;
            void plt_clean() {
                socdl.GetAddrInfoExCancel_(&cancel);
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
                if (res != 0) {
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

        static WaitObject* platform_resolve_address(const SockAddr& addr, int& err, Host& host, Port& port) {
            ADDRINFOEXW hint{};
            hint.ai_family = addr.af;
            hint.ai_socktype = addr.type;
            hint.ai_protocol = addr.proto;
            hint.ai_flags = addr.flag;
            timeval timeout;
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            auto obj = new_from_global_heap<WaitObject>();
            if (!obj) {
                err = no_resource;
                return nullptr;
            }
            quic::mem::RAII r{obj, [](auto obj) {
                                  delete_with_global_heap(obj);
                              }};
            auto event = CreateEventW(nullptr, true, false, nullptr);
            if (!event) {
                err = no_resource;
                return nullptr;
            }
            obj->ol.hEvent = event;
            auto res = socdl.GetAddrInfoExW_(host.c_str(), port.c_str(), 0, nullptr, &hint, &obj->info, &timeout, &obj->ol, nullptr, &obj->cancel);
            if (res != 0 && res != WSA_IO_PENDING) {
                err = get_error();
                return nullptr;
            }
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

        static WaitObject* platform_resolve_address(const SockAddr& addr, int& err, Host& host, Port& port) {
            auto obj = new_from_global_heap<WaitObject>();
            if (!obj) {
                err = no_resource;
                return nullptr;
            }
            quic::mem::RAII r{obj, [](auto obj) {
                                  delete_with_global_heap(obj);
                              }};
            obj->hint.ai_family = addr.af;
            obj->hint.ai_socktype = addr.type;
            obj->hint.ai_protocol = addr.proto;
            obj->hint.ai_flags = addr.flag;
            obj->cb.ar_request = &obj->hint;
            obj->host = std::move(host);
            obj->port = std::move(port);
            obj->cb.ar_name = obj->host.c_str();
            obj->cb.ar_service = obj->port.c_str();
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

        WaitAddrInfo::~WaitAddrInfo() {
            if (opt) {
                auto obj = static_cast<WaitObject*>(opt);
                delete_with_global_heap(obj);
            }
        }

        dll_internal(WaitAddrInfo) resolve_address(const SockAddr& addr, const char* port) {
            if (!init_sockdl()) {
                return {
                    nullptr,
                    libload_failed,
                };
            }
            Host host{};
            Port port_{};
            utf::convert(helper::SizedView(addr.hostname, addr.namelen), host);
            utf::convert(port, port_);
            int err = 0;
            auto obj = platform_resolve_address(addr, err, host, port_);
            if (!obj) {
                return {nullptr, err};
            }
            return {obj, 0};
        }
    }  // namespace dnet
}  // namespace utils
