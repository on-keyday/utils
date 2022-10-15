/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/crypto.h>
#include <dnet/tls.h>
#include <dnet/dll/ssldll.h>

namespace utils {
    namespace dnet {
        int quic_callback(TLS &tls, const quic::MethodArgs &args);
        namespace quic {

            int get_tls_then(ssl_import::SSL *ssl, auto &&then) {
                auto &s = ssldl;
                auto data = s.SSL_get_ex_data_(ssl, ssl_appdata_index);
                if (!data) {
                    return 0;
                }
                auto tls = static_cast<TLS *>(data);
                return then(*tls);
            }

            int set_encryption_secrets(ssl_import::SSL *ssl, ssl_import::quic_ext::OSSL_ENCRYPTION_LEVEL level,
                                       const byte *read_secret,
                                       const byte *write_secret, size_t secret_len) {
                return get_tls_then(ssl, [&](TLS &tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::secret;
                    arg.read_secret = read_secret;
                    arg.write_secret = write_secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return quic_callback(tls, arg);
                });
            }

            int set_write_secret(ssl_import::SSL *ssl, ssl_import::quic_ext::OSSL_ENCRYPTION_LEVEL level,
                                 const ssl_import::SSL_CIPHER *cipher, const uint8_t *secret,
                                 size_t secret_len) {
                return get_tls_then(ssl, [&](TLS &tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::wsecret;
                    arg.cipher = cipher;
                    arg.write_secret = secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return quic_callback(tls, arg);
                });
            }

            int set_read_secret(ssl_import::SSL *ssl, ssl_import::quic_ext::OSSL_ENCRYPTION_LEVEL level,
                                const ssl_import::SSL_CIPHER *cipher, const uint8_t *secret,
                                size_t secret_len) {
                return get_tls_then(ssl, [&](TLS &tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::rsecret;
                    arg.cipher = cipher;
                    arg.read_secret = secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return quic_callback(tls, arg);
                });
            }

            int add_handshake_data(ssl_import::SSL *ssl, ssl_import::quic_ext::OSSL_ENCRYPTION_LEVEL level,
                                   const byte *data, size_t len) {
                return get_tls_then(ssl, [&](TLS &tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::handshake_data;
                    arg.data = data;
                    arg.len = len;
                    arg.level = EncryptionLevel(level);
                    return quic_callback(tls, arg);
                });
            }

            int flush_flight(ssl_import::SSL *ssl) {
                return get_tls_then(ssl, [&](TLS &tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::flush;
                    return quic_callback(tls, arg);
                });
            }

            int send_alert(ssl_import::SSL *ssl, ssl_import::quic_ext::OSSL_ENCRYPTION_LEVEL level, byte alert) {
                return get_tls_then(ssl, [&](TLS &tls) {
                    MethodArgs arg{};
                    arg.type = ArgType::alert;
                    arg.alert = alert;
                    arg.level = EncryptionLevel(level);
                    return quic_callback(tls, arg);
                });
            }

            ssl_import::quic_ext::SSL_QUIC_METHOD_openssl quic_method_ossl{
                set_encryption_secrets,
                add_handshake_data,
                flush_flight,
                send_alert,
            };

            ssl_import::quic_ext::SSL_QUIC_METHOD_boringssl quic_method_bssl{
                set_read_secret,
                set_write_secret,
                add_handshake_data,
                flush_flight,
                send_alert,
            };

            dnet_dll_implement(bool) has_quic_ext() {
                auto res = load_ssl() && ssldl.load_quic_ext();
                return res;
            }

            bool set_quic_method(void *ptr) {
                auto ssl = static_cast<ssl_import::SSL *>(ptr);
                if (!has_quic_ext()) {
                    return false;
                }
                if (is_boringssl_ssl()) {
                    return (bool)ssldl.SSL_set_quic_method_(ssl, &quic_method_bssl);
                }
                else if (is_openssl_ssl()) {
                    return (bool)ssldl.SSL_set_quic_method_(ssl, &quic_method_ossl);
                }
                return false;
            }

        }  // namespace quic

    }  // namespace dnet
}  // namespace utils
