/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <quic/common/dll_cpp.h>
#include <quic/frame/decode.h>
#include <algorithm>
#include <quic/internal/external_func_internal.h>
#include <quic/internal/context_internal.h>
#include <quic/crypto/crypto.h>
#include <quic/mem/raii.h>

namespace utils {
    namespace quic {
        namespace crypto {
            constexpr auto quic_data_index = 1;

            int get_conn_then(SSL *ssl, auto &&then) {
                auto &s = external::libssl;
                auto data = s.SSL_get_ex_data_(ssl, quic_data_index);
                if (!data) {
                    return 0;
                }
                mem::RAII raw{
                    conn::Conn::from_raw_storage(data),
                    [](conn::Conn &conn) {
                        conn.get_storage(true);
                    }};
                return then(raw());
            }

            int set_encryption_secrets(SSL *ssl, OSSL_ENCRYPTION_LEVEL level,
                                       const byte *read_secret,
                                       const byte *write_secret, tsize secret_len) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    return 1;
                });
            }
            int add_handshake_data(SSL *ssl, OSSL_ENCRYPTION_LEVEL level,
                                   const byte *data, tsize len) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    return 1;
                });
            }

            int flush_flight(SSL *ssl) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    return 1;
                });
            }

            int send_alert(SSL *ssl, OSSL_ENCRYPTION_LEVEL level, byte alert) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    return 1;
                });
            }

            ::SSL_QUIC_METHOD quic_method{
                set_encryption_secrets,
                add_handshake_data,
                flush_flight,
                send_alert,
            };

            ::SSL *ssl(void *p) {
                return static_cast<::SSL *>(p);
            }

            ::SSL_CTX *sslctx(void *p) {
                return static_cast<::SSL_CTX *>(p);
            }

            Error initialize_ssl_context(conn::Conn &con) {
                auto &s = external::libssl;
                if (!con->ssl) {
                    if (!con->sslctx) {
                        con->sslctx = s.SSL_CTX_new_(s.TLS_method_());
                        if (!con->sslctx) {
                            return Error::memory_exhausted;
                        }
                    }
                    con->ssl = s.SSL_new_(sslctx(con->sslctx));
                    if (!con->ssl) {
                        return Error::memory_exhausted;
                    }
                    auto err = s.SSL_set_quic_method_(ssl(con->ssl), &quic_method);
                    if (err != 1) {
                        return Error::internal;
                    }
                    auto st = con.get_storage(true);  // increment storage use count
                    err = s.SSL_set_ex_data_(ssl(con->ssl), quic_data_index, st);
                    if (!err) {
                        con.from_raw_storage(st);  // decrement storage use count
                        return Error::internal;
                    }
                }
                return Error::none;
            }

            Error advance_handshake_core(const byte *data, tsize len, conn::Conn &con, EncryptionLevel level) {
                if (!external::load_LibSSL()) {
                    return Error::libload_failed;
                }
                if (auto err = initialize_ssl_context(con); err != Error::none) {
                    return err;
                }
                auto &s = external::libssl;
                // provide crypto data
                auto err = s.SSL_provide_quic_data_(ssl(con->ssl), OSSL_ENCRYPTION_LEVEL(level), data, len);
                if (err != 1) {
                    return Error::internal;
                }
                // advance handshake
                err = s.SSL_do_handshake_(ssl(con->ssl));
                auto code = s.SSL_get_error_(ssl(con->ssl), err);
                return Error::none;
            }

            dll(Error) advance_handshake(frame::Crypto &cframe, conn::Conn &con, EncryptionLevel level) {
                return advance_handshake_core(cframe.crypto_data.c_str(), cframe.length, con, level);
            }

            dll(Error) advance_handshake_vec(bytes::Buffer &buf, mem::Vec<frame::Crypto> &cframe, conn::Conn &con, EncryptionLevel level) {
                std::sort(cframe.begin(), cframe.end(), [&](frame::Crypto &a, frame::Crypto &b) {
                    return a.offset < b.offset;
                });
                for (auto &c : cframe) {
                    append(buf, c.crypto_data.c_str(), c.length);
                }
                return advance_handshake_core(buf.b.c_str(), buf.len, con, level);
            }
        }  // namespace crypto
    }      // namespace quic
}  // namespace utils
