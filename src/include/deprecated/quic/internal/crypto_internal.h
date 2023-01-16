/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto_internal - OpenSSL/BoringSSL export functions
#pragma once
/*
#if __has_include(<openssl/params.h>)
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/obj_mac.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#else*/
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
#define EVP_CTRL_GCM_SET_TAG 0x11
void ERR_print_errors_cb(int (*)(const char*, size_t, void*), void*);
/*
#endif
#if __has_include(<openssl/is_boringssl.h>)
#include <openssl/hkdf.h>
#else
*/
// boringssl
typedef void EVP_MD;
int HKDF_extract(uint8_t* out_key, size_t* out_len,
                 const EVP_MD* digest, const uint8_t* secret,
                 size_t secret_len, const uint8_t* salt,
                 size_t salt_len);

int HKDF_expand(uint8_t* out_key, size_t out_len,
                const EVP_MD* digest, const uint8_t* prk,
                size_t prk_len, const uint8_t* info,
                size_t info_len);
const EVP_MD* EVP_sha256(void);
typedef void SSL;
int SSL_set_tlsext_host_name(SSL* ssl, const char* name);
typedef void SSL_CIPHER;
/*
#endif
#if __has_include(<openssl/quic.h>)
#include <openssl/quic.h>
#if OPENSSL_INFO_QUIC >= 2000
#include <openssl/ssl.h>
#define EXISTS_OPENSSL_QUIC
#endif
#endif

#ifndef EXISTS_OPENSSL_QUIC
*/
typedef void SSL_CTX;
typedef void SSL;
void* SSL_CTX_new(const void*);
void* SSL_new(void*);
const void* TLS_method();
int SSL_set_ex_data(void*, int, void*);
void* SSL_get_ex_data(const void* s, int idx);
enum OSSL_ENCRYPTION_LEVEL {
    ssl_encryption_initial = 0,
    ssl_encryption_early_data,
    ssl_encryption_handshake,
    ssl_encryption_application
};
int SSL_provide_quic_data(SSL* ssl, OSSL_ENCRYPTION_LEVEL level, const uint8_t* data, size_t len);
int SSL_do_handshake(void*);
int SSL_get_error(const void*, int);
typedef void SSL_QUIC_METHOD;
int SSL_set_quic_method(void*, const SSL_QUIC_METHOD*);
void SSL_set_connect_state(void*);
void SSL_set_accept_state(void*);
#define SSL_ERROR_WANT_READ 2
int SSL_set_alpn_protos(void*, const unsigned char*, unsigned int);
#define SSL_CTRL_SET_TLSEXT_HOSTNAME 55
long SSL_ctrl(SSL*, int, long, void*);
int SSL_set_quic_transport_params(SSL* ssl, const uint8_t* params, size_t params_len);
int SSL_CTX_load_verify_locations(SSL_CTX* ctx, const char* CAfile, const char* CApath);
#define STACK_OF(Type) void
STACK_OF(X509_NAME) * SSL_load_client_CA_file(const char* file);
void SSL_CTX_set_client_CA_list(SSL_CTX* ctx, STACK_OF(X509_NAME) * list);
// #endif
// openssl style SSL_QUIC_METHOD
struct SSL_QUIC_METHOD_openssl {
    int (*set_encryption_secrets)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level,
                                  const uint8_t* read_secret,
                                  const uint8_t* write_secret, size_t secret_len);
    int (*add_handshake_data)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level,
                              const uint8_t* data, size_t len);
    int (*flush_flight)(SSL* ssl);
    int (*send_alert)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level, uint8_t alert);
};
// boringssl style SSL_QUIC_METHOD
struct SSL_QUIC_METHOD_boringssl {
    int (*set_read_secret)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level,
                           const SSL_CIPHER* cipher, const uint8_t* secret,
                           size_t secret_len);
    int (*set_write_secret)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level,
                            const SSL_CIPHER* cipher, const uint8_t* secret,
                            size_t secret_len);
    int (*add_handshake_data)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level,
                              const uint8_t* data, size_t len);
    int (*flush_flight)(SSL* ssl);
    int (*send_alert)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level, uint8_t alert);
};

namespace utils {
    namespace quic {
        namespace crypto {
#ifdef _WIN32
            constexpr auto default_libcrypto = "libcrypto.dll";
            constexpr auto default_libssl = "libssl.dll";
#else
            constexpr auto default_libcrypto = "libcrypto.so";
            constexpr auto default_libssl = "libssl.so";
#endif
        }  // namespace crypto
    }      // namespace quic
}  // namespace utils
