/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "lazy.h"
#include "sslfuncs.h"

namespace utils {
    namespace dnet::lazy {
        extern DLL libssl;
        extern DLL libcrypto;

#define LAZY(func) inline Func<decltype(ssl_import::common::func)> func##_(libssl, #func);

        LAZY(TLS_method)
        LAZY(SSL_CTX_new)
        LAZY(SSL_CTX_free)
        LAZY(SSL_CTX_up_ref)
        LAZY(SSL_new)
        LAZY(SSL_free)
        LAZY(SSL_set_ex_data)
        LAZY(SSL_get_ex_data)
        LAZY(SSL_get_error)
        LAZY(SSL_connect)
        LAZY(SSL_accept)
        LAZY(SSL_set_alpn_protos)
        LAZY(SSL_write)
        LAZY(SSL_read)
        LAZY(SSL_CTX_load_verify_locations)
        LAZY(SSL_load_client_CA_file)
        LAZY(SSL_CTX_set_client_CA_list)
        LAZY(SSL_set_bio)
        LAZY(SSL_shutdown)
        LAZY(SSL_set1_host)
        LAZY(SSL_CTX_use_certificate_chain_file)
        LAZY(SSL_CTX_use_PrivateKey_file)
        LAZY(SSL_CTX_check_private_key)
        LAZY(SSL_get_verify_result)
        LAZY(SSL_get0_alpn_selected)
        LAZY(SSL_CTX_set_verify)
        LAZY(SSL_get_current_cipher)
        LAZY(SSL_CIPHER_get_name)
        LAZY(SSL_CIPHER_standard_name)
        LAZY(SSL_CIPHER_get_bits)
        LAZY(SSL_CIPHER_get_cipher_nid)
        LAZY(SSL_set_hostflags)

#undef LAZY
#define LAZY(func) inline Func<decltype(ssl_import::quic_ext::func)> func##_(libssl, #func);

        LAZY(SSL_provide_quic_data)
        LAZY(SSL_set_quic_method)
        LAZY(SSL_set_quic_transport_params)
        LAZY(SSL_process_quic_post_handshake)
        LAZY(SSL_get_peer_quic_transport_params)
#undef LAZY
#define LAZY(func) inline Func<decltype(ssl_import::crypto::func)> func##_(libcrypto, #func);

        LAZY(ERR_print_errors_cb)
        LAZY(EVP_CIPHER_CTX_new)
        LAZY(EVP_aes_128_ecb)
        LAZY(EVP_aes_256_ecb)
        LAZY(EVP_aes_128_gcm)
        LAZY(EVP_get_cipherbynid)
        LAZY(EVP_CipherInit)
        LAZY(EVP_CipherInit_ex)
        LAZY(EVP_CipherUpdate)
        LAZY(EVP_CipherFinal_ex)
        LAZY(EVP_CIPHER_CTX_free)
        LAZY(EVP_CIPHER_CTX_ctrl)
        LAZY(BIO_new_bio_pair)
        LAZY(BIO_free_all)

#undef LAZY
#define LAZY(func) inline Func<decltype(ssl_import::open_ext::func)> func##_(libcrypto, #func);
        LAZY(CRYPTO_set_mem_functions)
        LAZY(BIO_test_flags)
        LAZY(BIO_read_ex)
        LAZY(BIO_write_ex)
        inline Func<decltype(ssl_import::open_ext::SSL_ctrl)> SSL_ctrl_(libssl, "SSL_ctrl");
#undef LAZY
#define LAZY(func) inline Func<decltype(ssl_import::open_quic_ext::func)> func##_(libcrypto, #func);
        LAZY(EVP_KDF_fetch)
        LAZY(EVP_KDF_CTX_new)
        LAZY(OSSL_PARAM_construct_end)
        LAZY(OSSL_PARAM_construct_utf8_string)
        LAZY(OSSL_PARAM_construct_octet_string)
        LAZY(EVP_KDF_CTX_free)
        LAZY(EVP_KDF_derive)
        LAZY(EVP_KDF_free)
        LAZY(EVP_KDF_CTX_get_kdf_size)
        LAZY(EVP_chacha20)
        LAZY(EVP_chacha20_poly1305)
#undef LAZY

#define LAZY(func) inline Func<decltype(ssl_import::boring_ext::func)> func##_(libcrypto, #func);
        LAZY(BIO_should_retry)
        LAZY(BIO_read)
        LAZY(BIO_write)
        LAZY(HKDF_extract)
        LAZY(HKDF_expand)
        LAZY(EVP_sha256)
        LAZY(CRYPTO_chacha_20)
        LAZY(EVP_aead_chacha20_poly1305)
        LAZY(EVP_AEAD_CTX_new)
        LAZY(EVP_AEAD_CTX_free)
        LAZY(EVP_AEAD_CTX_seal_scatter)
        LAZY(EVP_AEAD_CTX_open_gather)
        inline Func<decltype(ssl_import::boring_ext::SSL_set_tlsext_host_name)> SSL_set_tlsext_host_name_(libssl, "SSL_set_tlsext_host_name");
#undef LAZY
    }  // namespace dnet::lazy
}  // namespace utils
