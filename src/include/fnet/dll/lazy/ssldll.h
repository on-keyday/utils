/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "lazy.h"
#include "sslfuncs.h"

namespace futils {
    namespace fnet::lazy {
        extern DLL libssl;
        extern DLL libcrypto;
#define LAZY_BIND(dll, func_sig, func_name) inline Func<decltype(func_sig)> func_name##_{dll, #func_name};
        namespace ssl {
#define LAZY(func) LAZY_BIND(libssl, ssl_import::cc::ssl::func, func)
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
            LAZY(SSL_write)
            LAZY(SSL_read)
            LAZY(SSL_CTX_load_verify_locations)
            LAZY(SSL_load_client_CA_file)
            LAZY(SSL_set_bio)
            LAZY(SSL_shutdown)
            LAZY(SSL_set1_host)

            // for ctx
            LAZY(SSL_CTX_use_certificate_chain_file)
            LAZY(SSL_CTX_use_PrivateKey_file)
            LAZY(SSL_CTX_check_private_key)
            LAZY(SSL_CTX_set_client_CA_list)
            LAZY(SSL_CTX_set_verify)
            // for ssl
            LAZY(SSL_use_certificate_chain_file)
            LAZY(SSL_use_PrivateKey_file)
            LAZY(SSL_check_private_key)
            LAZY(SSL_set_client_CA_list)
            LAZY(SSL_set_verify)

            LAZY(SSL_get_verify_result)
            LAZY(SSL_get0_alpn_selected)
            LAZY(SSL_CTX_set_alpn_select_cb)

            LAZY(SSL_get_current_cipher)
            LAZY(SSL_CIPHER_get_name)
            LAZY(SSL_CIPHER_standard_name)
            LAZY(SSL_CIPHER_get_bits)
            LAZY(SSL_CIPHER_get_cipher_nid)
            LAZY(SSL_set_hostflags)
            LAZY(SSL_provide_quic_data)
            LAZY(SSL_set_quic_transport_params)
            LAZY(SSL_process_quic_post_handshake)
            LAZY(SSL_get_peer_quic_transport_params)

            LAZY(SSL_ctrl)

            LAZY(SSL_in_init)

            namespace ossl {
                LAZY_BIND(libssl, ssl_import::bc::open_ssl::ssl::SSL_CTX_set_alpn_protos, SSL_CTX_set_alpn_protos)
                LAZY_BIND(libssl, ssl_import::bc::open_ssl::ssl::SSL_set_alpn_protos, SSL_set_alpn_protos)
                LAZY_BIND(libssl, ssl_import::bc::open_ssl::ssl::SSL_set_quic_method, SSL_set_quic_method)
            }  // namespace ossl

            namespace bssl {
                LAZY_BIND(libssl, ssl_import::bc::boring_ssl::ssl::SSL_set_alpn_protos, SSL_set_alpn_protos)
                LAZY_BIND(libssl, ssl_import::bc::boring_ssl::ssl::SSL_CTX_set_alpn_protos, SSL_CTX_set_alpn_protos)
                LAZY_BIND(libssl, ssl_import::bc::boring_ssl::ssl::SSL_set_quic_method, SSL_set_quic_method)
                // specific
                namespace sp {
                    LAZY_BIND(libssl, ssl_import::ls::boring_ssl::ssl::SSL_CTX_get_ex_new_index, SSL_CTX_get_ex_new_index)
                    LAZY_BIND(libssl, ssl_import::ls::boring_ssl::ssl::SSL_set_tlsext_host_name, SSL_set_tlsext_host_name)
                    LAZY_BIND(libssl, ssl_import::ls::boring_ssl::ssl::SSL_error_description, SSL_error_description)
                }  // namespace sp
            }      // namespace bssl

            // early data support
            LAZY(SSL_set_quic_early_data_context)
            LAZY(SSL_set_early_data_enabled)
            LAZY(SSL_CTX_set_early_data_enabled)
            LAZY(SSL_early_data_accepted)
            LAZY(SSL_reset_early_data_reject)

            // session
            LAZY(SSL_set_session)
            LAZY(SSL_get_session)
            LAZY(SSL_CTX_sess_set_new_cb)
            LAZY(SSL_CTX_set_session_cache_mode)

            // get CTX for callback
            LAZY(SSL_get_SSL_CTX)

            LAZY(SSL_CTX_get_ex_data)
            LAZY(SSL_CTX_set_ex_data)
#undef LAZY
        }  // namespace ssl

        namespace crypto {
#define LAZY(func) LAZY_BIND(libcrypto, ssl_import::cc::crypto::func, func)
            LAZY(ERR_print_errors_cb)
            LAZY(ERR_clear_error)

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

            LAZY(BIO_test_flags)
            LAZY(BIO_read)
            LAZY(BIO_write)

            namespace ossl {
                LAZY_BIND(libcrypto, ssl_import::bc::open_ssl::crypto::ERR_get_error, ERR_get_error)
                LAZY_BIND(libcrypto, ssl_import::bc::open_ssl::crypto::ERR_lib_error_string, ERR_lib_error_string)
                LAZY_BIND(libcrypto, ssl_import::bc::open_ssl::crypto::ERR_reason_error_string, ERR_reason_error_string)

#undef LAZY
#define LAZY(func) LAZY_BIND(libcrypto, ssl_import::ls::open_ssl::crypto::func, func)
                // specific
                namespace sp {
                    LAZY(CRYPTO_get_ex_new_index)
                    LAZY(CRYPTO_set_mem_functions)
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
                }  // namespace sp
#undef LAZY
            }  // namespace ossl

            namespace bssl {
                LAZY_BIND(libcrypto, ssl_import::bc::boring_ssl::crypto::ERR_get_error, ERR_get_error)
                LAZY_BIND(libcrypto, ssl_import::bc::boring_ssl::crypto::ERR_lib_error_string, ERR_lib_error_string)
                LAZY_BIND(libcrypto, ssl_import::bc::boring_ssl::crypto::ERR_reason_error_string, ERR_reason_error_string)

#define LAZY(func) LAZY_BIND(libcrypto, ssl_import::ls::boring_ssl::crypto::func, func)
                // specific
                namespace sp {
                    LAZY(BIO_should_retry)
                    LAZY(HKDF_extract)
                    LAZY(HKDF_expand)
                    LAZY(EVP_sha256)
                    LAZY(CRYPTO_chacha_20)
                    LAZY(EVP_aead_chacha20_poly1305)
                    LAZY(EVP_AEAD_CTX_new)
                    LAZY(EVP_AEAD_CTX_free)
                    LAZY(EVP_AEAD_CTX_seal_scatter)
                    LAZY(EVP_AEAD_CTX_open_gather)
                }  // namespace sp
            }      // namespace bssl
#undef LAZY
        }  // namespace crypto

#undef LAZY_BIND
    }  // namespace fnet::lazy
}  // namespace futils
