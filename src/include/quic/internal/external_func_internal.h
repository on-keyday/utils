/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// external_func_internal - external function dependency
#pragma once
#include "../common/dll_h.h"
#include "sock_internal.h"
#include "crypto_internal.h"
#include <quic/mem/once.h>

namespace utils {
    namespace quic {
        namespace external {
#define IMPORT(FUNCNAME)                                                       \
    using FUNCNAME##_t = decltype(::FUNCNAME)*;                                \
    FUNCNAME##_t FUNCNAME##_;                                                  \
    FUNCNAME##_t Load##FUNCNAME(Loader load) {                                 \
        FUNCNAME##_ = reinterpret_cast<FUNCNAME##_t>(load(libptr, #FUNCNAME)); \
        return FUNCNAME##_;                                                    \
    }
#define LOAD(FUNCNAME)                          \
    auto ptr_##FUNCNAME = Load##FUNCNAME(load); \
    if (!ptr_##FUNCNAME) {                      \
        return false;                           \
    }

#ifdef _WIN32
            using Proc = FARPROC;
#else
            using Proc = void (*)();
#endif
            using Loader = Proc (*)(void* lib, const char* name);

            struct SockDll {
                void* libptr;
#ifdef _WIN32
                IMPORT(WSAStartup)
                IMPORT(WSASocketW)
                IMPORT(WSARecvFrom)
                IMPORT(WSACleanup)
                IMPORT(WSAGetLastError)
                IMPORT(ioctlsocket)
                IMPORT(closesocket)
#else
                IMPORT(close)
                IMPORT(ioctl)
#endif
                IMPORT(sendto)
                IMPORT(recvfrom)
                IMPORT(setsockopt)
                IMPORT(getsockopt)
                IMPORT(connect)
                IMPORT(bind)

                bool LoadAll(Loader load) {
#ifdef _WIN32
                    LOAD(WSAStartup)
                    LOAD(WSASocketW)
                    LOAD(WSARecvFrom)
                    LOAD(WSACleanup)
                    LOAD(WSAGetLastError)
                    LOAD(ioctlsocket)
                    LOAD(closesocket)
#else
                    LOAD(close)
                    LOAD(ioctl)
#endif
                    LOAD(sendto)
                    LOAD(recvfrom)
                    LOAD(setsockopt)
                    LOAD(getsockopt)
                    LOAD(connect)
                    LOAD(bind)
                    return true;
                }

                bool loaded() {
                    return libptr != nullptr;
                }
            };

            struct LibCrypto {
                void* libptr;

                // hkdf.cpp
                IMPORT(EVP_KDF_fetch)
                IMPORT(EVP_KDF_CTX_new)
                IMPORT(EVP_KDF_free)
                IMPORT(OSSL_PARAM_construct_utf8_string)
                IMPORT(OSSL_PARAM_construct_octet_string)
                IMPORT(OSSL_PARAM_construct_end)
                IMPORT(EVP_KDF_derive)
                IMPORT(EVP_KDF_CTX_free)
                IMPORT(EVP_KDF_CTX_get_kdf_size)

                // initial.cpp
                IMPORT(EVP_CIPHER_CTX_new)
                IMPORT(EVP_aes_128_ecb)
                IMPORT(EVP_aes_128_gcm)
                IMPORT(EVP_CipherInit)
                IMPORT(EVP_CipherInit_ex)
                IMPORT(EVP_CIPHER_CTX_ctrl)
                IMPORT(EVP_CipherUpdate)
                IMPORT(EVP_CipherFinal)
                IMPORT(EVP_CIPHER_CTX_free)

                bool LoadAll(Loader load) {
                    // hkdf.cpp
                    LOAD(EVP_KDF_fetch)
                    LOAD(EVP_KDF_CTX_new)
                    LOAD(EVP_KDF_free)
                    LOAD(OSSL_PARAM_construct_utf8_string)
                    LOAD(OSSL_PARAM_construct_octet_string)
                    LOAD(OSSL_PARAM_construct_end)
                    LOAD(EVP_KDF_derive)
                    LOAD(EVP_KDF_CTX_free)
                    LOAD(EVP_KDF_CTX_get_kdf_size)

                    // initial.cpp
                    LOAD(EVP_CIPHER_CTX_new)
                    LOAD(EVP_aes_128_ecb)
                    LOAD(EVP_aes_128_gcm)
                    LOAD(EVP_CipherInit)
                    LOAD(EVP_CipherInit_ex)
                    LOAD(EVP_CIPHER_CTX_ctrl)
                    LOAD(EVP_CipherUpdate)
                    LOAD(EVP_CipherFinal)
                    LOAD(EVP_CIPHER_CTX_free)

                    return true;
                }
            };

#ifdef _WIN32
            struct Kernel32 {
                void* libptr;
                IMPORT(CreateIoCompletionPort)
                IMPORT(GetQueuedCompletionStatus)

                bool LoadAll(Loader load) {
                    LOAD(CreateIoCompletionPort)
                    LOAD(GetQueuedCompletionStatus)
                    return true;
                }
            };
            bool load_Kernel32();
            extern Kernel32 kernel32;
#endif
            // not usable from application
            extern SockDll sockdll;
            extern LibCrypto libcrypto;

            template <class Dll>
            using DllLoadCallback = void (*)(Dll* dll, void* user);

            bool load_SockDll(DllLoadCallback<SockDll> callback, void* user);
            void unload_SockDll(DllLoadCallback<SockDll> callback, void* user);

            bool load_LibCrypto();
            Dll(void) set_libcrypto_location(const char* location);
        }  // namespace external
    }      // namespace quic
}  // namespace utils
