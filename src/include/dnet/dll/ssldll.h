/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dynload.h"

namespace ssl_import {
    using SSL_CTX = struct SSL_CTX;
    using SSL = struct SSL;
    using SSL_METHOD = struct SSL_METHOD;
    using SSL_CIPHER = struct SSL_CIPHER;
    using EVP_CIPHER_CTX = struct EVP_CIPHER_CTX;
    using EVP_CIPHER = struct EVP_CIPHER;
    using ENGINE = struct ENGINE;
    using OSSL_LIB_CTX = struct OSSL_LIB_CTX;
    using EVP_KDF = struct EVP_KDF;
    using EVP_KDF_CTX = struct EVP_KDF_CTX;
    using BIO = struct BIO;
    using X509_VERIFY_PARAM = struct X509_VERIFY_PARAM;
    using X509_STORE_CTX = struct X509_STORE_CTX;
    constexpr auto SSL_CTRL_SET_TLSEXT_HOSTNAME_ = 55;
    constexpr auto BIO_FLAGS_SHOULD_RETRY_ = 0x8;
    constexpr auto SSL_ERROR_WANT_READ_ = 0x2;
    constexpr auto SSL_ERROR_ZERO_RETURNED_ = 0x6;
    constexpr auto SSL_FILETYPE_PEM_ = 1;
    constexpr auto X509_V_OK_ = 0;
    constexpr auto EVP_CTRL_GCM_SET_IVLEN_ = 0x9;
    constexpr auto EVP_CTRL_GCM_GET_TAG_ = 0x10;
    constexpr auto EVP_CTRL_GCM_SET_TAG_ = 0x11;
    namespace common {
        const SSL_METHOD* TLS_method();
        SSL_CTX* SSL_CTX_new(const SSL_METHOD* meth);
        void SSL_CTX_free(SSL_CTX* ctx);
        int SSL_CTX_up_ref(SSL_CTX* ctx);

        SSL* SSL_new(SSL_CTX* ctx);
        void SSL_free(SSL* ssl);

        int SSL_set_ex_data(SSL* ssl, int idx, void* data);
        void* SSL_get_ex_data(const SSL* ssl, int idx);

        int SSL_do_handshake(SSL* ssl);
        int SSL_get_error(const SSL* ssl, int ret);
        void SSL_set_connect_state(SSL* ssl);
        void SSL_set_accept_state(SSL* ssl);
        int SSL_connect(SSL* ssl);
        int SSL_accept(SSL* ssl);
        int SSL_set_alpn_protos(SSL* ssl, const unsigned char* protos, unsigned int protos_len);
        int SSL_read(SSL* ssl, void* buf, int num);
        int SSL_write(SSL* ssl, const void* buf, int num);
        void SSL_set_bio(SSL* ssl, BIO* rbio, BIO* wbio);
        int SSL_shutdown(SSL* ssl);

#define STACK_OF(Type) void
        int SSL_CTX_load_verify_locations(SSL_CTX* ctx, const char* CAfile, const char* CApath);
        STACK_OF(X509_NAME) * SSL_load_client_CA_file(const char* file);
        void SSL_CTX_set_client_CA_list(SSL_CTX* ctx, STACK_OF(X509_NAME) * list);

#undef STACK_OF

        int SSL_set1_host(SSL* s, const char* hostname);
        int SSL_CTX_use_certificate_chain_file(SSL_CTX* ctx, const char* file);
        int SSL_CTX_use_PrivateKey_file(SSL_CTX* ctx, const char* file, int type);
        int SSL_CTX_check_private_key(const SSL_CTX* ctx);
        long SSL_get_verify_result(const SSL* ssl);
        void SSL_get0_alpn_selected(const SSL* ssl, const unsigned char** data,
                                    unsigned int* len);

        void SSL_CTX_set_verify(SSL_CTX* ctx, int mode,
                                int (*verify_callback)(int, X509_STORE_CTX*));
    }  // namespace common

    namespace crypto {
        void ERR_print_errors_cb(int (*)(const char*, size_t, void*), void*);

        EVP_CIPHER_CTX* EVP_CIPHER_CTX_new();
        const EVP_CIPHER* EVP_aes_128_ecb();
        const EVP_CIPHER* EVP_aes_128_gcm();
        int EVP_CipherInit(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* type,
                           const unsigned char* key, const unsigned char* iv, int enc);
        int EVP_CipherInit_ex(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* type,
                              ENGINE* impl, const unsigned char* key, const unsigned char* iv, int enc);
        int EVP_CipherUpdate(EVP_CIPHER_CTX* ctx, unsigned char* out,
                             int* outl, const unsigned char* in, int inl);
        int EVP_CipherFinal_ex(EVP_CIPHER_CTX* ctx, unsigned char* outm, int* outl);
        void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX* ctx);
        int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX* ctx, int cmd, int p1, void* p2);

        int BIO_new_bio_pair(BIO** bio1, size_t writebuf1, BIO** bio2, size_t writebuf2);
        void BIO_free_all(BIO* a);
    }  // namespace crypto

    namespace open_ext {
        long SSL_ctrl(SSL* ssl, int cmd, long larg, void* parg);
        int BIO_test_flags(const BIO* b, int flags);
        int BIO_read_ex(BIO* b, void* data, size_t dlen, size_t* readbytes);
        int BIO_write_ex(BIO* b, const void* data, size_t dlen, size_t* written);
        int CRYPTO_set_mem_functions(
            void* (*m)(size_t, const char*, int),
            void* (*r)(void*, size_t, const char*, int),
            void (*f)(void*, const char*, int));
    }  // namespace open_ext

    namespace boring_ext {
        using EVP_MD = void;
        int SSL_set_tlsext_host_name(SSL* ssl, const char* name);
        int BIO_should_retry(const BIO* bio);
        int HKDF_extract(uint8_t* out_key, size_t* out_len,
                         const EVP_MD* digest, const uint8_t* secret,
                         size_t secret_len, const uint8_t* salt,
                         size_t salt_len);

        int HKDF_expand(uint8_t* out_key, size_t out_len,
                        const EVP_MD* digest, const uint8_t* prk,
                        size_t prk_len, const uint8_t* info,
                        size_t info_len);
        const EVP_MD* EVP_sha256(void);
        int BIO_read(BIO* bio, void* buf, int len);
        int BIO_write(BIO* bio, const void* data, size_t len);
    }  // namespace boring_ext

    namespace open_quic_ext {
        EVP_KDF* EVP_KDF_fetch(OSSL_LIB_CTX* libctx, const char* algorithm,
                               const char* properties);

        EVP_KDF_CTX* EVP_KDF_CTX_new(const EVP_KDF* kdf);
        struct OSSL_PARAM {
            void* opaque1;
            unsigned int opaque2;
            void* opaque3;
            size_t opaque4;
            size_t opaque5;
        };
        OSSL_PARAM OSSL_PARAM_construct_end();
        OSSL_PARAM OSSL_PARAM_construct_utf8_string(const char* key, char* buf, size_t bsize);
        OSSL_PARAM OSSL_PARAM_construct_octet_string(const char* key, unsigned char* buf, size_t bsize);
        void EVP_KDF_CTX_free(EVP_KDF_CTX* ctx);
        int EVP_KDF_derive(EVP_KDF_CTX* ctx, unsigned char* key, size_t keylen, const OSSL_PARAM* params);
        void EVP_KDF_free(EVP_KDF* kdf);
        size_t EVP_KDF_CTX_get_kdf_size(EVP_KDF_CTX* ctx);
    }  // namespace open_quic_ext

    namespace quic_ext {

        enum OSSL_ENCRYPTION_LEVEL {
            ssl_encryption_initial = 0,
            ssl_encryption_early_data,
            ssl_encryption_handshake,
            ssl_encryption_application
        };
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
        using SSL_QUIC_METHOD = void;
        int SSL_provide_quic_data(SSL* ssl, OSSL_ENCRYPTION_LEVEL level, const uint8_t* data, size_t len);
        int SSL_set_quic_method(SSL* ssl, const SSL_QUIC_METHOD* meth);
        int SSL_set_quic_transport_params(SSL* ssl, const uint8_t* params, size_t params_len);
        int SSL_process_quic_post_handshake(SSL* ssl);
    }  // namespace quic_ext
}  // namespace ssl_import

namespace utils {
    namespace dnet {
        struct SSLDll {
           private:
            alib<29> libsslcommon;
#define L(func) LOADER_BASE(ssl_import::common::func, func, libsslcommon, SSLDll, false)
            // common
            L(TLS_method)
            L(SSL_CTX_new)
            L(SSL_CTX_free)
            L(SSL_CTX_up_ref)
            L(SSL_new)
            L(SSL_free)
            L(SSL_set_ex_data)
            L(SSL_get_ex_data)
            L(SSL_do_handshake)
            L(SSL_get_error)
            L(SSL_set_connect_state)
            L(SSL_set_accept_state)
            L(SSL_connect)
            L(SSL_accept)
            L(SSL_set_alpn_protos)
            L(SSL_write)
            L(SSL_read)
            L(SSL_CTX_load_verify_locations)
            L(SSL_load_client_CA_file)
            L(SSL_CTX_set_client_CA_list)
            L(SSL_set_bio)
            L(SSL_shutdown)
            L(SSL_set1_host)
            L(SSL_CTX_use_certificate_chain_file)
            L(SSL_CTX_use_PrivateKey_file)
            L(SSL_CTX_check_private_key)
            L(SSL_get_verify_result)
            L(SSL_get0_alpn_selected)
            L(SSL_CTX_set_verify)
#undef L
           private:
            alib_nptr<4> libquic;
#define L(func) LOADER_BASE(ssl_import::quic_ext::func, func, libquic, SSLDll, false)
            // quic externsions
            L(SSL_provide_quic_data)
            L(SSL_set_quic_method)
            L(SSL_set_quic_transport_params)
            L(SSL_process_quic_post_handshake)
#undef L
           private:
            alib<13> libcryptocommon;
#define L(func) LOADER_BASE(ssl_import::crypto::func, func, libcryptocommon, SSLDll, false)
            // libcrypto functions
            L(ERR_print_errors_cb)
            L(EVP_CIPHER_CTX_new)
            L(EVP_aes_128_ecb)
            L(EVP_aes_128_gcm)
            L(EVP_CipherInit)
            L(EVP_CipherInit_ex)
            L(EVP_CipherUpdate)
            L(EVP_CipherFinal_ex)
            L(EVP_CIPHER_CTX_free)
            L(EVP_CIPHER_CTX_ctrl)
            L(BIO_new_bio_pair)
            L(BIO_free_all)
            // for memory hook
            LOADER_BASE(ssl_import::open_ext::CRYPTO_set_mem_functions, CRYPTO_set_mem_functions, libcryptocommon, SSLDll, true)
#undef L
           private:
            alib_nptr<1> osslext_ssl;
#define L(func) LOADER_BASE(ssl_import::open_ext::func, func, osslext_ssl, SSLDll, false)
            L(SSL_ctrl)
#undef L
           private:
            alib_nptr<3> osslext_crypto;
#define L(func) LOADER_BASE(ssl_import::open_ext::func, func, osslext_crypto, SSLDll, false)
            // openssl externsions
            L(BIO_test_flags)
            L(BIO_read_ex)
            L(BIO_write_ex)
#undef L
           private:
            alib_nptr<9> osslquicext_crypto;
#define L(func) LOADER_BASE(ssl_import::open_quic_ext::func, func, osslquicext_crypto, SSLDll, false)
            L(EVP_KDF_fetch)
            L(EVP_KDF_CTX_new)
            L(OSSL_PARAM_construct_end)
            L(OSSL_PARAM_construct_utf8_string)
            L(OSSL_PARAM_construct_octet_string)
            L(EVP_KDF_CTX_free)
            L(EVP_KDF_derive)
            L(EVP_KDF_free)
            L(EVP_KDF_CTX_get_kdf_size)
#undef L
           private:
            alib_nptr<1> bsslext_ssl;
#define L(func) LOADER_BASE(ssl_import::boring_ext::func, func, bsslext_ssl, SSLDll, false)
            L(SSL_set_tlsext_host_name)
#undef L
           private:
            alib_nptr<6> bsslext_crypto;
#define L(func) LOADER_BASE(ssl_import::boring_ext::func, func, bsslext_crypto, SSLDll, false)
            L(BIO_should_retry)
            L(BIO_read)
            L(BIO_write)
            L(HKDF_extract)
            L(HKDF_expand)
            L(EVP_sha256)
#undef L
           private:
#ifdef _WIN32
#define DL_SUFFIX ".dll"
#else
#define DL_SUFFIX ".so"
#endif
            const char* libssl_mod = "libssl" DL_SUFFIX;
            const char* libcrypto_mod = "libcrypto" DL_SUFFIX;
#undef DL_SUFFIX
           public:
            constexpr void set_libssl(const char* mod) {
                libssl_mod = mod;
            }
            constexpr void set_libcrypto(const char* mod) {
                libcrypto_mod = mod;
            }
            bool load_ssl_common();
            bool load_crypto_common();

            // call load_ssl_common before call this function
            bool load_quic_ext();
            // call load_ssl_common before call this function
            bool load_boringssl_ssl_ext();
            // call load_crypto_common before call this function
            bool load_boringssl_crypto_ext();
            // call load_ssl_common before call this function
            bool load_openssl_ssl_ext();
            // call load_crypto_common before call this function
            bool load_openssl_quic_ext();
            // call load_crypto_common before call this function
            bool load_openssl_crypto_ext();

           private:
            static SSLDll instance;
            friend SSLDll& get_ssldll_instance();
            constexpr SSLDll() {}
            ~SSLDll();
        };

        extern SSLDll& ssldl;

        constexpr auto ssl_appdata_index = 0;
    }  // namespace dnet
}  // namespace utils
#undef LOADER_BASE
