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
#include <deprecated/quic/mem/once.h>

namespace utils {
    namespace quic {
        namespace external {
#define IMPORT(FUNCNAME) \
    decltype(::FUNCNAME)* FUNCNAME##_;

#define LOAD_ONLY(FUNCNAME) \
    FUNCNAME##_ = reinterpret_cast<decltype(FUNCNAME)*>(load(libptr, #FUNCNAME));

#define LOAD(FUNCNAME)            \
    LOAD_ONLY(FUNCNAME)           \
    if (FUNCNAME##_ == nullptr) { \
        return false;             \
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
                bool is_boring_ssl;

                // hkdf.cpp
                // judge library by this function
                IMPORT(EVP_KDF_fetch)
                union {
                    // openssl
                    struct {
                        IMPORT(EVP_KDF_CTX_new)
                        IMPORT(EVP_KDF_free)
                        IMPORT(OSSL_PARAM_construct_utf8_string)
                        IMPORT(OSSL_PARAM_construct_octet_string)
                        IMPORT(OSSL_PARAM_construct_end)
                        IMPORT(EVP_KDF_derive)
                        IMPORT(EVP_KDF_CTX_free)
                    };
                    // boringssl
                    struct {
                        IMPORT(EVP_sha256)
                        IMPORT(HKDF_extract)
                        IMPORT(HKDF_expand)
                    };
                };

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

                // crypto_frames.cpp
                IMPORT(ERR_print_errors_cb)

                bool LoadAll(Loader load) {
                    // hkdf.cpp
                    LOAD_ONLY(EVP_KDF_fetch)
                    if (EVP_KDF_fetch_) {
                        // normal openssl code
                        LOAD(EVP_KDF_CTX_new)
                        LOAD(EVP_KDF_free)
                        LOAD(OSSL_PARAM_construct_utf8_string)
                        LOAD(OSSL_PARAM_construct_octet_string)
                        LOAD(OSSL_PARAM_construct_end)
                        LOAD(EVP_KDF_derive)
                        LOAD(EVP_KDF_CTX_free)
                    }
                    else {
                        // try to load boringssl api
                        LOAD(HKDF_expand)
                        LOAD(HKDF_extract)
                        LOAD(EVP_sha256)
                        // now loading boring ssl!
                        is_boring_ssl = true;
                    }

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

                    // crypto_frames.cpp
                    LOAD(ERR_print_errors_cb)

                    return true;
                }
            };

            struct LibSSL {
                void* libptr;
                bool is_boring_ssl;

                IMPORT(SSL_CTX_new)
                IMPORT(SSL_new)
                IMPORT(TLS_method)
                IMPORT(SSL_set_quic_method)
                IMPORT(SSL_set_ex_data)
                IMPORT(SSL_get_ex_data)
                IMPORT(SSL_provide_quic_data)
                IMPORT(SSL_do_handshake)
                IMPORT(SSL_get_error)
                IMPORT(SSL_set_connect_state)
                IMPORT(SSL_set_accept_state)
                IMPORT(SSL_set_alpn_protos)
                union {
                    IMPORT(SSL_ctrl)
                    IMPORT(SSL_set_tlsext_host_name)
                };
                IMPORT(SSL_set_quic_transport_params)
                IMPORT(SSL_CTX_load_verify_locations)
                IMPORT(SSL_load_client_CA_file)
                IMPORT(SSL_CTX_set_client_CA_list)

                bool LoadAll(Loader load) {
                    LOAD(SSL_CTX_new)
                    LOAD(SSL_new)
                    LOAD(TLS_method)
                    LOAD(SSL_set_quic_method)
                    LOAD(SSL_set_ex_data)
                    LOAD(SSL_get_ex_data)
                    LOAD(SSL_provide_quic_data)
                    LOAD(SSL_do_handshake)
                    LOAD(SSL_get_error)
                    LOAD(SSL_set_connect_state)
                    LOAD(SSL_set_accept_state)
                    LOAD(SSL_set_alpn_protos)
                    LOAD_ONLY(SSL_ctrl)
                    if (!SSL_ctrl_) {
                        LOAD(SSL_set_tlsext_host_name)
                        is_boring_ssl = true;
                    }
                    LOAD(SSL_set_quic_transport_params)
                    LOAD(SSL_CTX_load_verify_locations)
                    LOAD(SSL_load_client_CA_file)
                    LOAD(SSL_CTX_set_client_CA_list)
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
            extern LibSSL libssl;

            template <class Dll>
            using DllLoadCallback = void (*)(Dll* dll, void* user);

            bool load_SockDll(DllLoadCallback<SockDll> callback, void* user);
            void unload_SockDll(DllLoadCallback<SockDll> callback, void* user);

            Dll(bool) load_LibCrypto();
            Dll(bool) load_LibSSL();
            Dll(void) set_libcrypto_location(const char* location);
            Dll(void) set_libssl_location(const char* location);
        }  // namespace external
    }      // namespace quic
}  // namespace utils
