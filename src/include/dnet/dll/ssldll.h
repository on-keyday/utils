/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dynload.h"
#include "sslfuncs.h"

namespace utils {
    namespace dnet {
        struct SSLDll {
           private:
            alib<32> libsslcommon;
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
            L(SSL_get_error)
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
            L(SSL_get_current_cipher)
            L(SSL_CIPHER_get_name)
            L(SSL_CIPHER_standard_name)
            L(SSL_CIPHER_get_bits)
            L(SSL_CIPHER_get_cipher_nid)
            L(SSL_set_hostflags)
#undef L
           private:
            alib_nptr<5> libquic;
#define L(func) LOADER_BASE(ssl_import::quic_ext::func, func, libquic, SSLDll, false)
            // quic externsions
            L(SSL_provide_quic_data)
            L(SSL_set_quic_method)
            L(SSL_set_quic_transport_params)
            L(SSL_process_quic_post_handshake)
            L(SSL_get_peer_quic_transport_params)
#undef L
           private:
            alib<15> libcryptocommon;
#define L(func) LOADER_BASE(ssl_import::crypto::func, func, libcryptocommon, SSLDll, false)
            // libcrypto functions
            L(ERR_print_errors_cb)
            L(EVP_CIPHER_CTX_new)
            L(EVP_aes_128_ecb)
            L(EVP_aes_256_ecb)
            L(EVP_aes_128_gcm)
            L(EVP_get_cipherbynid)
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
            alib_nptr<11> osslquicext_crypto;
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
            L(EVP_chacha20)
            L(EVP_chacha20_poly1305)
#undef L
           private:
            alib_nptr<1> bsslext_ssl;
#define L(func) LOADER_BASE(ssl_import::boring_ext::func, func, bsslext_ssl, SSLDll, false)
            L(SSL_set_tlsext_host_name)
#undef L
           private:
            alib_nptr<12> bsslext_crypto;
#define L(func) LOADER_BASE(ssl_import::boring_ext::func, func, bsslext_crypto, SSLDll, false)
            L(BIO_should_retry)
            L(BIO_read)
            L(BIO_write)
            L(HKDF_extract)
            L(HKDF_expand)
            L(EVP_sha256)
            L(CRYPTO_chacha_20)
            L(EVP_aead_chacha20_poly1305)
            L(EVP_AEAD_CTX_new)
            L(EVP_AEAD_CTX_free)
            L(EVP_AEAD_CTX_seal_scatter)
            L(EVP_AEAD_CTX_open_gather)
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
