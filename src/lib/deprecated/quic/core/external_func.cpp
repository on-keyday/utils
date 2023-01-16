/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <deprecated/quic/common/dll_cpp.h>
#include <deprecated/quic/internal/external_func_internal.h>

#ifdef _WIN32
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace utils {
    namespace quic {
        namespace external {

#ifdef _WIN32
            void* get_self_module_handle() {
                return &__ImageBase;
            }

            constexpr auto socket_dll = "ws2_32.dll";
            auto loader(void* lib, const char* name) {
                return ::GetProcAddress(static_cast<HMODULE>(lib), name);
            }

            auto load_system_dll(const char* name) {
                return ::LoadLibraryExA(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            }

            auto load_normal_dll(const char* name) {
                return ::LoadLibraryA(name);
            }

            void unload_dll(void* libptr) {
                ::FreeLibrary(static_cast<HMODULE>(libptr));
            }

            Kernel32 kernel32;
            static mem::Once kernelonce;

            bool load_Kernel32() {
                kernelonce.Do([&] {
                    kernel32.libptr = load_system_dll("kernel32.dll");
                    if (!kernel32.libptr) {
                        return;
                    }
                    if (!kernel32.LoadAll(loader)) {
                        unload_dll(kernel32.libptr);
                        kernel32.libptr = nullptr;
                    }
                });
                return kernel32.libptr != nullptr;
            }
#else
            void* load_system_dll(const char* name) {
                return nullptr;
            }

            void* load_normal_dll(const char* name) {
                return nullptr;
            }
            auto loader(void* lib, const char* name) {
                return (void (*)()) nullptr;
            }

            void unload_dll(void*) {}

            constexpr auto socket_dll = "";
#endif

            SockDll sockdll;
            static mem::Once sockonce;
            bool load_SockDll(DllLoadCallback<SockDll> callback, void* user) {
                sockonce.Do([&] {
                    sockdll.libptr = load_system_dll(socket_dll);
                    if (!sockdll.libptr) {
                        return;
                    }
                    if (!sockdll.LoadAll(loader)) {
                        unload_dll(sockdll.libptr);
                        sockdll.libptr = nullptr;
                        callback(nullptr, user);
                        return;
                    }
                    callback(&sockdll, user);
                    return;
                });
                return sockdll.libptr != nullptr;
            }

            void unload_SockDll(DllLoadCallback<SockDll> callback, void* user) {
                sockonce.Undo([&] {
                    callback(&sockdll, user);
                    unload_dll(sockdll.libptr);
                    sockdll.libptr = nullptr;
                });
            }

            LibCrypto libcrypto;
            static mem::Once cryptoonce;
            static const char* libcrypto_loc;

            template <class Lib>
            bool load_normal(mem::Once& once, Lib& lib, const char* loc, const char* defloc) {
                once.Do([&] {
                    if (!loc) {
                        loc = defloc;
                    }
                    lib.libptr = load_normal_dll(loc);
                    if (!lib.libptr) {
                        return;
                    }
                    if (!lib.LoadAll(loader)) {
                        unload_dll(lib.libptr);
                        lib.libptr = nullptr;
                        return;
                    }
                });
                return lib.libptr != nullptr;
            }

            dll(bool) load_LibCrypto() {
                return load_normal(cryptoonce, libcrypto, libcrypto_loc, crypto::default_libcrypto);
            }

            dll(void) set_libcrypto_location(const char* location) {
                libcrypto_loc = location;
            }

            LibSSL libssl;
            static mem::Once sslonce;
            static const char* libssl_loc;

            dll(bool) load_LibSSL() {
                return load_normal(sslonce, libssl, libssl_loc, crypto::default_libssl);
            }

            dll(void) set_libssl_location(const char* location) {
                libssl_loc = location;
            }
        }  // namespace external
    }      // namespace quic
}  // namespace utils
