/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/crypto/crypto.h>
#include <dnet/tls/tls.h>
#include <dnet/dll/lazy/ssldll.h>

namespace utils {
    namespace dnet {
        // defined at tls.cpp
        namespace tls {
            TLSCipher make_cipher(const void *c);
            int quic_callback(void *c, const quic::crypto::MethodArgs &args);
            tls::TLSCipher get_quic_cipher(void *);
        }  // namespace tls

        namespace quic::crypto {

            int get_tls_then(ssl_import::SSL *ssl, auto &&then) {
                auto data = lazy::ssl::SSL_get_ex_data_(ssl, ssl_import::ssl_appdata_index);
                if (!data) {
                    return 0;
                }
                // auto tls = static_cast<tls::TLS *>(data);
                return then(data);
            }

            int set_encryption_secrets(ssl_import::SSL *ssl, ssl_import::OSSL_ENCRYPTION_LEVEL level,
                                       const byte *read_secret,
                                       const byte *write_secret, size_t secret_len) {
                return get_tls_then(ssl, [&](void *tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::secret;
                    arg.read_secret = read_secret;
                    arg.write_secret = write_secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    arg.cipher = tls::get_quic_cipher(tls);
                    return tls::quic_callback(tls, arg);
                });
            }

            int set_write_secret(ssl_import::SSL *ssl, ssl_import::OSSL_ENCRYPTION_LEVEL level,
                                 const ssl_import::SSL_CIPHER *cipher, const uint8_t *secret,
                                 size_t secret_len) {
                return get_tls_then(ssl, [&](void *tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::wsecret;
                    arg.cipher = tls::make_cipher(cipher);
                    arg.write_secret = secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return tls::quic_callback(tls, arg);
                });
            }

            int set_read_secret(ssl_import::SSL *ssl, ssl_import::OSSL_ENCRYPTION_LEVEL level,
                                const ssl_import::SSL_CIPHER *cipher, const uint8_t *secret,
                                size_t secret_len) {
                return get_tls_then(ssl, [&](void *data) {
                    MethodArgs arg{};
                    arg.type = ArgType::rsecret;
                    arg.cipher = tls::make_cipher(cipher);
                    arg.read_secret = secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return tls::quic_callback(data, arg);
                });
            }

            int add_handshake_data(ssl_import::SSL *ssl, ssl_import::OSSL_ENCRYPTION_LEVEL level,
                                   const byte *data, size_t len) {
                return get_tls_then(ssl, [&](void *tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::handshake_data;
                    arg.data = data;
                    arg.len = len;
                    arg.level = EncryptionLevel(level);
                    return tls::quic_callback(tls, arg);
                });
            }

            int flush_flight(ssl_import::SSL *ssl) {
                return get_tls_then(ssl, [&](void *tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::flush;
                    return tls::quic_callback(tls, arg);
                });
            }

            int send_alert(ssl_import::SSL *ssl, ssl_import::OSSL_ENCRYPTION_LEVEL level, byte alert) {
                return get_tls_then(ssl, [&](void *tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::alert;
                    arg.alert = alert;
                    arg.level = EncryptionLevel(level);
                    return tls::quic_callback(tls, arg);
                });
            }

            ssl_import::bc::open_ssl::ssl::SSL_QUIC_METHOD quic_method_ossl{
                set_encryption_secrets,
                add_handshake_data,
                flush_flight,
                send_alert,
            };

            ssl_import::bc::boring_ssl::ssl::SSL_QUIC_METHOD quic_method_bssl{
                set_read_secret,
                set_write_secret,
                add_handshake_data,
                flush_flight,
                send_alert,
            };

            dnet_dll_implement(bool) has_quic_ext() {
                if (tls::is_boring_ssl()) {  // boring ssl
                    return (bool)lazy::ssl::bssl::SSL_set_quic_method_.find();
                }
                else if (tls::is_open_ssl()) {  // open ssl
                    return (bool)lazy::ssl::ossl::SSL_set_quic_method_.find();
                }
                return false;
            }

            bool set_quic_method(void *ptr) {
                auto ssl = static_cast<ssl_import::SSL *>(ptr);
                if (!has_quic_ext()) {
                    return false;
                }
                if (tls::is_boring_ssl()) {  // boring ssl method
                    return (bool)lazy::ssl::bssl::SSL_set_quic_method_(ssl, &quic_method_bssl);
                }
                else if (tls::is_open_ssl()) {  // open ssl method
                    return (bool)lazy::ssl::ossl::SSL_set_quic_method_(ssl, &quic_method_ossl);
                }
                return false;
            }

        }  // namespace quic::crypto

    }  // namespace dnet
}  // namespace utils
