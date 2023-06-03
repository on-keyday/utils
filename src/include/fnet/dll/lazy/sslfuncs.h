/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// sslfuncs - OpenSSL/BoringSSL function forward declration
// because this library mentioning about OpenSSL and to avoid lawsuit
// these two note is written
// * This product includes cryptographic software written by Eric Young (eay@cryptsoft.com)
// * This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)
// but unfortunately, this library uses no source code or binary written by OpenSSL except only forward declarations
// forward declarations are copied by myself, sorted,and changed some details
#pragma once
#include <cstddef>
#include <cstdint>

namespace ssl_import {
#define STACK_OF(Type) STACK_OF_##Type
    using SSL_CTX = struct SSL_CTX;
    using SSL = struct SSL;
    using SSL_METHOD = struct SSL_METHOD;
    using SSL_CIPHER = struct SSL_CIPHER;
    using SSL_SESSION = struct SSL_SESSION;
    using EVP_CIPHER_CTX = struct EVP_CIPHER_CTX;
    using EVP_CIPHER = struct EVP_CIPHER;
    using ENGINE = struct ENGINE;
    using OSSL_LIB_CTX = struct OSSL_LIB_CTX;
    using EVP_KDF = struct EVP_KDF;
    using EVP_KDF_CTX = struct EVP_KDF_CTX;
    using BIO = struct BIO;
    using X509_VERIFY_PARAM = struct X509_VERIFY_PARAM;
    using X509_STORE_CTX = struct X509_STORE_CTX;

    using CRYPTO_EX_DATA = struct CRYPTO_EX_DATA;
    using CRYPTO_EX_free = void(void* parent, void* ptr, CRYPTO_EX_DATA* ad,
                                int index, long argl, void* argp);

    using CRYPTO_EX_dup = void();
    using CRYPTO_EX_unused = void;

    // boringssl
    using EVP_AEAD = struct EVP_AEAD;
    using EVP_AEAD_CTX = struct EVP_AEAD_CTX;
    using EVP_MD = struct EVP_MD;

    using STACK_OF(X509_NAME) = struct STACK_OF(X509_NAME);

    constexpr auto SSL_CTRL_SET_TLSEXT_HOSTNAME_ = 55;
    constexpr auto BIO_FLAGS_SHOULD_RETRY_ = 0x8;
    constexpr auto SSL_ERROR_WANT_READ_ = 0x2;
    constexpr auto SSL_ERROR_ZERO_RETURNED_ = 0x6;
    constexpr auto SSL_FILETYPE_PEM_ = 1;
    constexpr auto X509_V_OK_ = 0;
    constexpr auto EVP_CTRL_AEAD_SET_IVLEN_ = 0x9;
    constexpr auto EVP_CTRL_AEAD_GET_TAG_ = 0x10;
    constexpr auto EVP_CTRL_AEAD_SET_TAG_ = 0x11;
    constexpr auto X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS_ = 0x4;

    constexpr auto SSL_TLSEXT_ERR_OK_ = 0;
    constexpr auto SSL_TLSEXT_ERR_ALERT_WARNING_ = 1;
    constexpr auto SSL_TLSEXT_ERR_ALERT_FATAL_ = 2;
    constexpr auto SSL_TLSEXT_ERR_NOACK_ = 3;

    constexpr auto SSL_SESS_CACHE_CLIENT_ = 0x0001;
    constexpr auto SSL_SESS_CACHE_SERVER_ = 0x0002;

    // OpenSSL specific
    constexpr auto CRYPTO_EX_INDEX_SSL_CTX_ = 1;

    // platform requirement
    static_assert(sizeof(unsigned char) == sizeof(std::uint8_t));

    enum OSSL_ENCRYPTION_LEVEL {
        ssl_encryption_initial = 0,
        ssl_encryption_early_data,
        ssl_encryption_handshake,
        ssl_encryption_application
    };

    // completely compatible (both OpenSSL and BoringSSL has same signature)
    // at BoringSSL,some function argument declared as `unsigned` type
    // but `unsigned` = `unsigned int` so compatibility is not broken
    namespace cc {
        namespace ssl {
            const SSL_METHOD* TLS_method();
            SSL_CTX* SSL_CTX_new(const SSL_METHOD* meth);
            void SSL_CTX_free(SSL_CTX* ctx);
            int SSL_CTX_up_ref(SSL_CTX* ctx);

            SSL* SSL_new(SSL_CTX* ctx);
            void SSL_free(SSL* ssl);

            int SSL_get_error(const SSL* ssl, int ret);

            int SSL_connect(SSL* ssl);
            int SSL_accept(SSL* ssl);

            int SSL_read(SSL* ssl, void* buf, int num);
            int SSL_write(SSL* ssl, const void* buf, int num);
            void SSL_set_bio(SSL* ssl, BIO* rbio, BIO* wbio);
            int SSL_shutdown(SSL* ssl);

            int SSL_CTX_load_verify_locations(SSL_CTX* ctx, const char* CAfile, const char* CApath);
            STACK_OF(X509_NAME) * SSL_load_client_CA_file(const char* file);

            int SSL_set1_host(SSL* s, const char* hostname);
            int SSL_add1_host(SSL* s, const char* hostname);
            void SSL_set_hostflags(SSL* s, unsigned int flags);

            // ex data (on fnet, for QUIC callback)
            int SSL_set_ex_data(SSL* ssl, int idx, void* data);
            void* SSL_get_ex_data(const SSL* ssl, int idx);

            int SSL_CTX_set_ex_data(SSL_CTX* ssl, int idx, void* data);
            void* SSL_CTX_get_ex_data(const SSL_CTX* ssl, int idx);

            // for SSL_CTX*
            int SSL_CTX_use_certificate_chain_file(SSL_CTX* ctx, const char* file);
            int SSL_CTX_use_PrivateKey_file(SSL_CTX* ctx, const char* file, int type);
            int SSL_CTX_check_private_key(const SSL_CTX* ctx);
            void SSL_CTX_set_verify(SSL_CTX* ctx, int mode,
                                    int (*verify_callback)(int, X509_STORE_CTX*));
            void SSL_CTX_set_client_CA_list(SSL_CTX* ctx, STACK_OF(X509_NAME) * list);
            // for SSL*
            int SSL_use_certificate_chain_file(SSL* ctx, const char* file);
            int SSL_use_PrivateKey_file(SSL* ctx, const char* file, int type);
            int SSL_check_private_key(const SSL* ctx);
            void SSL_set_verify(SSL* ctx, int mode,
                                int (*verify_callback)(int, X509_STORE_CTX*));

            void SSL_set_client_CA_list(SSL* ctx, STACK_OF(X509_NAME) * list);

            // ALPN
            void SSL_get0_alpn_selected(const SSL* ssl, const unsigned char** data,
                                        unsigned int* len);
            void SSL_CTX_set_alpn_select_cb(
                SSL_CTX* ctx, int (*cb)(SSL* ssl, const uint8_t** out, uint8_t* out_len, const uint8_t* in, unsigned in_len, void* arg),
                void* arg);

            long SSL_get_verify_result(const SSL* ssl);

            const SSL_CIPHER* SSL_get_current_cipher(const SSL* ssl);
            const char* SSL_CIPHER_get_name(const SSL_CIPHER* cipher);
            const char* SSL_CIPHER_standard_name(const SSL_CIPHER* cipher);
            int SSL_CIPHER_get_bits(const SSL_CIPHER* cipher, int* alg_bits);
            int SSL_CIPHER_get_cipher_nid(const SSL_CIPHER* c);

            // for openssl macro function
            long SSL_ctrl(SSL* ssl, int cmd, long larg, void* parg);

            int SSL_provide_quic_data(SSL* ssl, OSSL_ENCRYPTION_LEVEL level, const uint8_t* data, size_t len);
            int SSL_set_quic_transport_params(SSL* ssl, const uint8_t* params, size_t params_len);
            int SSL_process_quic_post_handshake(SSL* ssl);
            void SSL_get_peer_quic_transport_params(const SSL* ssl, const uint8_t** out_params, size_t* out_params_len);

            // early data (0-RTT) support
            int SSL_set_quic_early_data_context(SSL* ssl,
                                                const uint8_t* context,
                                                size_t context_len);
            void SSL_set_early_data_enabled(SSL* ssl, int enabled);
            void SSL_CTX_set_early_data_enabled(SSL_CTX* ctx, int enabled);
            int SSL_early_data_accepted(const SSL* ssl);
            void SSL_reset_early_data_reject(SSL* ssl);

            // session (for 0RTT)
            int SSL_set_session(SSL*, SSL_SESSION* sess);
            SSL_SESSION* SSL_get_session(const SSL* ssl);
            void SSL_CTX_sess_set_new_cb(
                SSL_CTX* ctx, int (*new_session_cb)(SSL* ssl, SSL_SESSION* session));
            int SSL_CTX_set_session_cache_mode(SSL_CTX* ctx, int mode);

            // get CTX for callback
            SSL_CTX* SSL_get_SSL_CTX(const SSL* ssl);

        }  // namespace ssl

        namespace crypto {
            void ERR_print_errors_cb(int (*)(const char*, size_t, void*), void*);
            void ERR_clear_error();

            EVP_CIPHER_CTX* EVP_CIPHER_CTX_new();
            const EVP_CIPHER* EVP_aes_128_ecb();
            const EVP_CIPHER* EVP_aes_256_ecb();
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

            const EVP_CIPHER* EVP_get_cipherbynid(int nid);

            int BIO_test_flags(const BIO* b, int flags);
            int BIO_read(BIO* bio, void* buf, int len);
            int BIO_write(BIO* bio, const void* data, int len);

        }  // namespace crypto

    }  // namespace cc

    // broken compatibilty (maybe diffrent integer type is used between OpenSSL and BoringSSL)
    // maybe ABI level is compatible but require API compatiblity for safety
    namespace bc {
        namespace open_ssl {
            namespace ssl {
                int SSL_CTX_set_alpn_protos(SSL_CTX* ssl, const unsigned char* protos, unsigned int protos_len);
                int SSL_set_alpn_protos(SSL* ssl, const unsigned char* protos, unsigned int protos_len);
                struct SSL_QUIC_METHOD {
                    int (*set_encryption_secrets)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level,
                                                  const uint8_t* read_secret,
                                                  const uint8_t* write_secret, size_t secret_len);
                    int (*add_handshake_data)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level,
                                              const uint8_t* data, size_t len);
                    int (*flush_flight)(SSL* ssl);
                    int (*send_alert)(SSL* ssl, OSSL_ENCRYPTION_LEVEL level, uint8_t alert);
                };
                int SSL_set_quic_method(SSL* ssl, const SSL_QUIC_METHOD* meth);
            }  // namespace ssl

            namespace crypto {
                unsigned long ERR_get_error();
                const char* ERR_lib_error_string(unsigned long e);
                const char* ERR_reason_error_string(unsigned long e);
            }  // namespace crypto

        }  // namespace open_ssl

        namespace boring_ssl {
            namespace ssl {

                int SSL_CTX_set_alpn_protos(SSL_CTX* ssl, const std::uint8_t* protos, size_t protos_len);
                int SSL_set_alpn_protos(SSL* ssl, const std::uint8_t* protos, size_t protos_len);

                struct SSL_QUIC_METHOD {
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
                int SSL_set_quic_method(SSL* ssl, const SSL_QUIC_METHOD* meth);
            }  // namespace ssl

            namespace crypto {
                std::uint32_t ERR_get_error();
                const char* ERR_lib_error_string(std::uint32_t e);
                const char* ERR_reason_error_string(std::uint32_t e);
            }  // namespace crypto

        }  // namespace boring_ssl

    }  // namespace bc

    // library specific
    namespace ls {
        namespace open_ssl {
            namespace ssl {

            }  // namespace ssl
            namespace crypto {
                int CRYPTO_get_ex_new_index(int class_index, long argl, void* argp,
                                            CRYPTO_EX_unused* unused,
                                            CRYPTO_EX_dup* dup_unused,
                                            CRYPTO_EX_free* free_func);
                int CRYPTO_set_mem_functions(
                    void* (*m)(size_t, const char*, int),
                    void* (*r)(void*, size_t, const char*, int),
                    void (*f)(void*, const char*, int));

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
                const EVP_CIPHER* EVP_chacha20();
                const EVP_CIPHER* EVP_chacha20_poly1305();
            }  // namespace crypto
        }      // namespace open_ssl

        namespace boring_ssl {
            namespace ssl {
                int SSL_set_tlsext_host_name(SSL* ssl, const char* name);
                const char* SSL_error_description(int err);
                int SSL_CTX_get_ex_new_index(long argl, void* argp,
                                             CRYPTO_EX_unused* unused,
                                             CRYPTO_EX_dup* dup_unused,
                                             CRYPTO_EX_free* free_func);

            }  // namespace ssl

            namespace crypto {
                int BIO_should_retry(const BIO* bio);
                const EVP_MD* EVP_sha256(void);

                void CRYPTO_chacha_20(uint8_t* out, const uint8_t* in,
                                      size_t in_len, const uint8_t key[32],
                                      const uint8_t nonce[12], uint32_t counter);

                int HKDF_extract(uint8_t* out_key, size_t* out_len,
                                 const EVP_MD* digest, const uint8_t* secret,
                                 size_t secret_len, const uint8_t* salt,
                                 size_t salt_len);

                int HKDF_expand(uint8_t* out_key, size_t out_len,
                                const EVP_MD* digest, const uint8_t* prk,
                                size_t prk_len, const uint8_t* info,
                                size_t info_len);

                const EVP_AEAD* EVP_aead_chacha20_poly1305(void);
                EVP_AEAD_CTX* EVP_AEAD_CTX_new(const EVP_AEAD* aead,
                                               const uint8_t* key,
                                               size_t key_len, size_t tag_len);
                void EVP_AEAD_CTX_free(EVP_AEAD_CTX* ctx);

                int EVP_AEAD_CTX_seal_scatter(
                    const EVP_AEAD_CTX* ctx, uint8_t* out,
                    uint8_t* out_tag, size_t* out_tag_len, size_t max_out_tag_len,
                    const uint8_t* nonce, size_t nonce_len,
                    const uint8_t* in, size_t in_len,
                    const uint8_t* extra_in, size_t extra_in_len,
                    const uint8_t* ad, size_t ad_len);

                int EVP_AEAD_CTX_open_gather(
                    const EVP_AEAD_CTX* ctx, uint8_t* out, const uint8_t* nonce,
                    size_t nonce_len, const uint8_t* in, size_t in_len, const uint8_t* in_tag,
                    size_t in_tag_len, const uint8_t* ad, size_t ad_len);

            }  // namespace crypto
        }      // namespace boring_ssl
    }          // namespace ls

    constexpr size_t ssl_appdata_index = 0;
}  // namespace ssl_import
#undef STACK_OF
