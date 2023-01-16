/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/ssldll.h>
#include <number/array.h>
#include <utf/convert.h>
#include <memory>
#include <dnet/dll/glheap.h>

namespace utils {
    namespace dnet {
        SSLDll SSLDll::instance;
        SSLDll& get_ssldll_instance() {
            return SSLDll::instance;
        }

        SSLDll& ssldl = get_ssldll_instance();

        bool load_lib(auto this_, auto& lib, void*& libh, auto libname, bool use_exists) {
            auto libp = std::addressof(lib);
            return load_library_impl(
                this_, libp, [](void* this_, void* lh, void* lp, loader_t loader) -> bool {
                    return static_cast<decltype(libp)>(lp)->load_all(lh, this_, loader);
                },
                libh, libname, false, use_exists);
        }

        void init_mem_alloc() {
            // hook allocation for openssl
            // boringssl has no way to hook allocation dynamic
            if (ssldl.CRYPTO_set_mem_functions_) {
                ssldl.CRYPTO_set_mem_functions_(
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
            }
        }

        bool SSLDll::load_ssl_common() {
            if (!libssl_mod) {
                libssl_mod = "libssl";
            }
#ifdef _WIN32
            number::Array<wchar_t, 1024, true> buf{};
            utf::convert(libssl_mod, buf);
            return load_lib(this, libsslcommon, libsslcommon.libp, buf.c_str(), false);
#else
            return load_lib(this, libsslcommon, libsslcommon.libp, libssl_mod, false);
#endif
        }

        bool SSLDll::load_crypto_common() {
            if (!libcrypto_mod) {
                libcrypto_mod = "libcrypto";
            }
#ifdef _WIN32
            number::Array<wchar_t, 1024, true> buf{};
            utf::convert(libcrypto_mod, buf);
            auto res = load_lib(this, libcryptocommon, libcryptocommon.libp, buf.c_str(), false);
#else
            auto res = load_lib(this, libcryptocommon, libcryptocommon.libp, libcrypto_mod, false);
#endif
            init_mem_alloc();
            return res;
        }

        // call load_ssl_common before call this function
        bool SSLDll::load_quic_ext() {
            return load_lib(this, libquic, libsslcommon.libp, nullptr, true);
        }
        // call load_ssl_common before call this function
        bool SSLDll::load_boringssl_ssl_ext() {
            return load_lib(this, bsslext_ssl, libsslcommon.libp, nullptr, true);
        }
        // call load_crypto_common before call this function
        bool SSLDll::load_boringssl_crypto_ext() {
            return load_lib(this, bsslext_crypto, libcryptocommon.libp, nullptr, true);
        }
        // call load_ssl_common before call this function
        bool SSLDll::load_openssl_ssl_ext() {
            return load_lib(this, osslext_ssl, libsslcommon.libp, nullptr, true);
        }

        // call load_crypto_common before call this function
        bool SSLDll::load_openssl_quic_ext() {
            return load_lib(this, osslquicext_crypto, libcryptocommon.libp, nullptr, true);
        }

        // call load_crypto_common before call this function
        bool SSLDll::load_openssl_crypto_ext() {
            return load_lib(this, osslext_crypto, libcryptocommon.libp, nullptr, true);
        }

        void release_mod(void* mod);

        SSLDll::~SSLDll() {
            release_mod(libcryptocommon.libp);
            release_mod(libsslcommon.libp);
        }
    }  // namespace dnet
}  // namespace utils
