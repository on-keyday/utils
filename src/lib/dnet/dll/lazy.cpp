/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/lazy/lazy.h>
#include <dnet/plthead.h>
#include <dnet/dll/lazy/sockdll.h>
#include <view/iovec.h>
#include <dnet/dll/lazy/ssldll.h>
#include <dnet/dll/glheap.h>
#include <helper/pushbacker.h>
#include <helper/appender.h>

namespace utils {
    namespace dnet::lazy {
        func_t DLLInit::lookup(const char* name) {
            return dll->lookup_platform(name);
        }
        void (*call_fail)(const char* fn) = nullptr;
        void set_call_fail_traits(void (*f)(const char* fn)) {
            call_fail = f;
        }
        void call_fail_traits(const char* fn) {
            auto f = call_fail;
            if (f) {
                f(fn);
            }
            abort();
        }

        bool libcrypto_init(utils::dnet::lazy::DLLInit l, bool on_load) {
            if (!on_load) {
                return true;
            }
            // hook allocation for openssl
            // boringssl has no way to hook allocation dynamic
            const auto mem_hook = l.lookup<decltype(ssl_import::open_ext::CRYPTO_set_mem_functions)>("CRYPTO_set_mem_functions");
            if (!mem_hook) {
                return true;
            }
            mem_hook(
                [](size_t s, const char* file, int line) {
                    return alloc_normal(
                        s,
                        alignof(std::max_align_t), DebugInfo{
                                                       .size_known = true,
                                                       .size = s,
                                                       .align = alignof(std::max_align_t),
                                                       .file = file,
                                                       .line = line,
                                                       .func = "OPENSSL_malloc",
                                                   });
                },
                [](void* p, size_t s, const char* file, int line) {
                    return realloc_normal(
                        p, s,
                        alignof(std::max_align_t), DebugInfo{
                                                       .size_known = false,
                                                       .size = s,
                                                       .align = alignof(std::max_align_t),
                                                       .file = file,
                                                       .line = line,
                                                       .func = "OPENSSL_realloc",
                                                   });
                },
                [](void* p, const char* file, int line) {
                    return free_normal(p, DebugInfo{
                                              .size_known = false,
                                              .size = 0,
                                              .align = alignof(std::max_align_t),
                                              .file = file,
                                              .line = line,
                                              .func = "OPENSSL_free",
                                          });
                });
            return true;
        }

        void log_fn_load(const char* fn, bool ok) {
            char buf[120]{};
            helper::CharVecPushbacker<char> pb{buf, 119};
            helper::appends(pb, fn, ok ? " loaded" : " unavailable", "\n");
            OutputDebugStringA(buf);
        }

#ifdef _WIN32
        bool DLL::load_platform() {
            if (sys) {
                target = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            }
            else {
                target = LoadLibraryExW(path, 0, 0);
            }
            return target != nullptr;
        }

        void DLL::unload_platform() {
            if (!target) {
                return;
            }
            FreeLibrary((HMODULE)target);
            target = nullptr;
        }

        func_t DLL::lookup_platform(const char* func) {
            return func_t(GetProcAddress((HMODULE)target, func));
        }

        WSADATA wsadata;

        bool network_init(DLLInit dll, bool on_load) {
            if (!on_load) {
                const auto WSACleanup_ = dll.lookup<decltype(WSACleanup)>("WSACleanup");
                if (!WSACleanup_) {
                    return false;
                }
                WSACleanup_();
                return true;
            }
            const auto WSAStartup_ = dll.lookup<decltype(WSAStartup)>("WSAStartup");
            if (!WSAStartup_) {
                return false;
            }
            if (WSAStartup_(MAKEWORD(2, 2), &wsadata) != 0) {
                return false;
            }
            return true;
        }

        std::pair<func_t, bool> network_lookup(DLLInit dll, const char* name) {
            auto get_fn = [&](GUID guid, auto fn_type) -> std::pair<func_t, bool> {
                // in here, look up with lock is safe
                auto tmp = WSASocketW_(AF_INET, SOCK_STREAM, 0, nullptr, 0, 0);
                if (tmp == -1) {
                    return {func_t{}, true};
                }
                const auto d = helper::defer([&] {
                    closesocket_(tmp);
                });
                DWORD dw = 0;
                WSAIoctl_(tmp, SIO_GET_EXTENSION_FUNCTION_POINTER,
                          &guid, sizeof(guid),
                          &fn_type, sizeof(fn_type),
                          &dw, NULL, NULL);
                return {func_t(fn_type), true};
            };
            if (view::rvec(name) == "ConnectEx") {
                return get_fn(WSAID_CONNECTEX, LPFN_CONNECTEX{});
            }
            if (view::rvec(name) == "AcceptEx") {
                return get_fn(WSAID_ACCEPTEX, LPFN_ACCEPTEX{});
            }
            return {nullptr, false};
        }

        bool no_set_file_notif = false;

        std::pair<func_t, bool> kernel32_lookup(DLLInit init, const char* name) {
            if (view::rvec(name) == "SetFileCompletionNotificationModes") {
                if (no_set_file_notif) {
                    return {nullptr, true};
                }
                auto fn = init.lookup(name);
                if (!fn) {
                    no_set_file_notif = true;
                    return {nullptr, true};
                }
                INT ptr[]{IPPROTO_TCP, 0};
                WSAPROTOCOL_INFOW info[32];
                DWORD len = sizeof(info);
                if (WSAEnumProtocolsW_(ptr, info, &len) == SOCKET_ERROR) {
                    no_set_file_notif = true;
                    return {nullptr, true};
                }
                for (auto i = 0; i < len; i++) {
                    if ((info[i].dwServiceFlags1 & XP1_IFS_HANDLES) == 0) {
                        no_set_file_notif = true;
                        return {nullptr, true};
                    }
                }
                return {fn, true};
            }
            return {nullptr, false};
        }

        DLL ws2_32{L"ws2_32.dll", true, network_init, network_lookup};
        DLL kernel32{L"kernel32.dll", true, nullptr, kernel32_lookup};
        DLL libssl{L"libssl.dll", false};
        DLL libcrypto{L"libcrypto.dll", false, libcrypto_init};
#else
        DLL libanl{"libanl.so", false};
        DLL libssl{"libssl.so", false};
        DLL libcrypto{"libcrypto.so", false, libcrypto_init};
#endif

    }  // namespace dnet::lazy
}  // namespace utils
