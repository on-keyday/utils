/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/sockdll.h>
#include <utility>
#include <memory>
#ifndef _WIN32
#include <dlfcn.h>
#include <sys/stat.h>
#include <quic/mem/raii.h>
#endif
namespace utils {
    namespace dnet {

        void* load_sys_mod(const void* libname);
        void* load_mod(const void* libname);
        void release_mod(void* mod);

        func_t loader(void* libptr, const char* fname);

        bool load_library_impl(
            void* this_, void* libp,
            load_all_t load_all, void*& libh,
            const void* libname, bool syslib, bool use_exists) {
            if (use_exists) {
                if (!libh) {
                    return false;
                }
            }
            else {
                if (libh) {
                    return true;
                }
                if (syslib) {
                    libh = load_sys_mod(libname);
                }
                else {
                    libh = load_mod(libname);
#ifndef _WIN32
                    if (!libh) {
                        return false;
                    }
#endif
                }
#ifdef _WIN32
                if (!libh) {
                    return false;
                }
#endif
            }
            if (!load_all(this_, libh, libp, loader)) {
                if (!use_exists) {
                    release_mod(libh);
                    libh = nullptr;
                }
                return false;
            }
            return true;
        }

        bool load_system_lib(auto this_, auto& lib, auto libname, bool sys = true) {
            auto libp = std::addressof(lib);
            return load_library_impl(
                this_, libp, [](void* this_, void* lh, void* lp, loader_t loader) -> bool {
                    return static_cast<decltype(libp)>(lp)->load_all(this_, loader);
                },
                lib.libp, libname, sys, false);
        }

        SocketDll SocketDll::instance;
        SocketDll& get_sock_instance() {
            return SocketDll::instance;
        }
        SocketDll& socdl = get_sock_instance();

        Kernel Kernel::instance;

        Kernel& get_ker32_instacne() {
            return Kernel::instance;
        }

        Kernel& kerlib = get_ker32_instacne();

#ifdef _WIN32

        void* load_sys_mod(const void* libname) {
            auto name = static_cast<const wchar_t*>(libname);
            return LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        }

        void* load_mod(const void* libname) {
            auto name = static_cast<const wchar_t*>(libname);
            return LoadLibraryW(name);
        }

        func_t loader(void* libptr, const char* fname) {
            return (func_t)GetProcAddress(HMODULE(libptr), fname);
        }

        void release_mod(void* mod) {
            FreeLibrary(HMODULE(mod));
        }

        WSADATA initdata;
        bool loaded = false;

        SocketDll::~SocketDll() {
            if (lib.libp) {
                if (loaded) {
                    WSACleanup_();
                }
                release_mod(lib.libp);
            }
        }

        bool SocketDll::load() {
            if (!load_system_lib(this, lib, L"ws2_32.dll")) {
                return false;
            }
            auto res = WSAStartup_(MAKEWORD(2, 2), &initdata);
            if (res != 0) {
                return false;
            }
            loaded = true;
            // load AcceptEx
            auto tmp = socket_(AF_INET, SOCK_STREAM, 0);
            if (tmp == -1) {
                return false;
            }
            DWORD dw = 0;
            GUID guid = WSAID_ACCEPTEX;
            res = WSAIoctl_(tmp, SIO_GET_EXTENSION_FUNCTION_POINTER,
                            &guid, sizeof(guid),
                            &AcceptEx_, sizeof(AcceptEx_),
                            &dw, NULL, NULL);
            if (!AcceptEx_) {
                return false;
            }
            // load ConnectEx
            guid = WSAID_CONNECTEX;
            res = WSAIoctl_(tmp, SIO_GET_EXTENSION_FUNCTION_POINTER,
                            &guid, sizeof(guid),
                            &ConnectEx_, sizeof(ConnectEx_),
                            &dw, NULL, NULL);
            if (!ConnectEx_) {
                return false;
            }
            closesocket_(tmp);
            return true;
        }

        bool Kernel::load() {
            return load_system_lib(this, lib, L"kernel32.dll");
        }
#else
        void* load_sys_mod(const void* libname) {
            return RTLD_DEFAULT;
        }

        void* load_mod(const void* libname) {
            auto name = static_cast<const char*>(libname);
            return dlopen(name, RTLD_LAZY | RTLD_GLOBAL);
        }

        func_t loader(void* libptr, const char* fname) {
            return (func_t)dlsym(libptr, fname);
        }

        void release_mod(void* mod) {
            if (mod) {
                auto debug_ = dlclose(mod);
                auto debug_2 = debug_ ? debug_ : debug_;
            }
        }

        SocketDll::~SocketDll() {
            release_mod(lib.libp);
        }

        bool SocketDll::load() {
            if (!load_system_lib(this, lib, "libc.so")) {
                return false;
            }
            return true;
        }

        bool Kernel::load() {
            quic::mem::RAII r{
                realpath(libanl, nullptr),
                [](auto c) {
                    free(c);
                },
            };
            auto libname = libanl;
            if (r()) {
                libname = r();
            }
            return load_system_lib(this, lib, libname, false);
        }
#endif
    }  // namespace dnet
}  // namespace utils
