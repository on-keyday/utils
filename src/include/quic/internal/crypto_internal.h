/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto_internal - OpenSSL includes
#pragma once
#if __has_include(<openssl/params.h>)
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/obj_mac.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#else
#include <cstddef>
// define dummy symbols
// hkdf.cpp
void* EVP_KDF_fetch(void*, const char*, void*);
void* EVP_KDF_CTX_new(void*);
struct OSSL_PARAM {
    void* opaque1;
    unsigned int opaque2;
    void* opaque3;
    size_t opaque4;
    size_t opaque5;
};
OSSL_PARAM OSSL_PARAM_construct_end();
void EVP_KDF_CTX_free(void*);
int EVP_KDF_derive(void*, void*, size_t, OSSL_PARAM*);
OSSL_PARAM OSSL_PARAM_construct_utf8_string(const void*, char*, size_t);
OSSL_PARAM OSSL_PARAM_construct_octet_string(const void*, unsigned char*, size_t);
void EVP_KDF_free(void*);
void EVP_KDF_CTX_free(void*);
size_t EVP_KDF_CTX_get_kdf_size(void*);
// initial.cpp
void* EVP_CIPHER_CTX_new();
const void* EVP_aes_128_ecb();
const void* EVP_aes_128_gcm();
int EVP_CipherInit(void*, const void*, const unsigned char*, const unsigned char*, int);
int EVP_CipherUpdate(void*, unsigned char*, int*, const unsigned char*, int);
int EVP_CipherFinal(void*, unsigned char*, int*);
void EVP_CIPHER_CTX_free(void*);
int EVP_CipherInit_ex(void*, const void*, void*, const unsigned char*, const unsigned char*, int);
int EVP_CIPHER_CTX_ctrl(void*, int, int, void*);
#define EVP_CTRL_GCM_SET_IVLEN 0x9
#endif

namespace utils {
    namespace quic {
        namespace crypto {
#ifdef _WIN32
            constexpr auto default_libcrypto = "libcrypto.dll";
#else
            constexpr auto default_libcrypto = "libcrypto.so";
#endif
        }  // namespace crypto
    }      // namespace quic
}  // namespace utils
