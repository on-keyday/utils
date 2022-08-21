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
#include <quic/mem/callback.h>

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

            enum class ArgType {
                secret,   // openssl
                wsecret,  // boring ssl
                rsecret,  // boring ssl
                handshake_data,
                flush,
                alert,
            };

            struct Args {
                ArgType type;
                EncryptionLevel level;
                const void *cipher;
                union {
                    const byte *read_secret;
                    const byte *data;
                };
                const byte *write_secret;
                union {
                    tsize len;
                    tsize secret_len;
                    byte alert;
                };
            };

            int set_encryption_secrets(SSL *ssl, OSSL_ENCRYPTION_LEVEL level,
                                       const byte *read_secret,
                                       const byte *write_secret, tsize secret_len) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    Args arg{};
                    arg.type = ArgType::secret;
                    arg.read_secret = read_secret;
                    arg.write_secret = write_secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return conn->tmp_callback(&arg);
                });
            }

            int set_write_secret(SSL *ssl, OSSL_ENCRYPTION_LEVEL level,
                                 const SSL_CIPHER *cipher, const uint8_t *secret,
                                 size_t secret_len) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    Args arg{};
                    arg.type = ArgType::wsecret;
                    arg.cipher = cipher;
                    arg.write_secret = secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return conn->tmp_callback(&arg);
                });
            }

            int set_read_secret(SSL *ssl, OSSL_ENCRYPTION_LEVEL level,
                                const SSL_CIPHER *cipher, const uint8_t *secret,
                                size_t secret_len) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    Args arg{};
                    arg.type = ArgType::rsecret;
                    arg.cipher = cipher;
                    arg.read_secret = secret;
                    arg.secret_len = secret_len;
                    arg.level = EncryptionLevel(level);
                    return conn->tmp_callback(&arg);
                });
            }

            int add_handshake_data(SSL *ssl, OSSL_ENCRYPTION_LEVEL level,
                                   const byte *data, tsize len) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    Args arg{};
                    arg.type = ArgType::handshake_data;
                    arg.data = data;
                    arg.len = len;
                    arg.level = EncryptionLevel(level);
                    return conn->tmp_callback(&arg);
                });
            }

            int flush_flight(SSL *ssl) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    Args arg{};
                    arg.type = ArgType::flush;
                    return conn->tmp_callback(&arg);
                });
            }

            int send_alert(SSL *ssl, OSSL_ENCRYPTION_LEVEL level, byte alert) {
                return get_conn_then(ssl, [&](conn::Conn &conn) {
                    conn->alert = alert;
                    Args arg{};
                    arg.type = ArgType::alert;
                    arg.alert = alert;
                    arg.level = EncryptionLevel(level);
                    return conn->tmp_callback(&arg);
                });
            }

            ::SSL_QUIC_METHOD_openssl quic_method_ossl{
                set_encryption_secrets,
                add_handshake_data,
                flush_flight,
                send_alert,
            };

            ::SSL_QUIC_METHOD_boringssl quic_method_bssl{
                set_read_secret,
                set_write_secret,
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
                if (!external::load_LibSSL()) {
                    return Error::libload_failed;
                }
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
                    int err = 0;
                    if (s.is_boring_ssl) {
                        err = s.SSL_set_quic_method_(ssl(con->ssl), reinterpret_cast<const SSL_QUIC_METHOD *>(&quic_method_bssl));
                    }
                    else {
                        err = s.SSL_set_quic_method_(ssl(con->ssl), reinterpret_cast<const SSL_QUIC_METHOD *>(&quic_method_ossl));
                    }
                    if (err != 1) {
                        return Error::internal;
                    }
                    auto st = con.get_storage(true);  // increment storage use count
                    err = s.SSL_set_ex_data_(ssl(con->ssl), quic_data_index, st);
                    if (!err) {
                        con.from_raw_storage(st);  // decrement storage use count
                        return Error::internal;
                    }
                    if (con->mode == client) {
                        s.SSL_set_connect_state_(ssl(con->ssl));
                    }
                    else {
                        s.SSL_set_accept_state_(ssl(con->ssl));
                    }
                }
                return Error::none;
            }
#define INIT_CTX(c)                                                 \
    if (auto err = initialize_ssl_context(c); err != Error::none) { \
        return err;                                                 \
    }

            dll(Error) set_transport_parameter(conn::Connection *c, const tpparam::DefinedParams *params, bool force_all,
                                               const tpparam::TransportParam *custom, tsize custom_len) {
                if (!c || !params) {
                    return Error::invalid_argument;
                }
                auto self = c->self;
                INIT_CTX(self)
                auto &s = external::libssl;
                bytes::Buffer b_r{};
                b_r.a = &c->q->g->alloc;
                auto err = tpparam::encode(b_r, *params, force_all);
                mem::RAII b{std::move(b_r), [&](bytes::Buffer &b) {
                                self->q->g->bpool.put(std::move(b.b));
                            }};
                if (err != varint::Error::none) {
                    return Error::invalid_argument;
                }
                if (custom) {
                    for (tsize i = 0; i < custom_len; i++) {
                        err = tpparam::encode(b(), custom[i], force_all);
                        if (err != varint::Error::none) {
                            return Error::invalid_argument;
                        }
                    }
                }
                auto ierr = s.SSL_set_quic_transport_params_(ssl(c->ssl), b().b.c_str(), b().len);
                if (!ierr) {
                    return Error::internal;
                }
                return Error::none;
            }

            dll(Error) set_hostname(conn::Connection *c, const char *host) {
                if (!c || !host) {
                    return Error::invalid_argument;
                }
                INIT_CTX(c->self)
                auto &s = external::libssl;
                int err;
                if (s.is_boring_ssl) {
                    err = s.SSL_set_tlsext_host_name_(ssl(c->ssl), host);
                }
                else {
                    err = s.SSL_ctrl_(ssl(c->ssl), SSL_CTRL_SET_TLSEXT_HOSTNAME, 0, (void *)host);
                }
                if (!err) {
                    return Error::internal;
                }
                return Error::none;
            }

            dll(Error) set_alpn(conn::Connection *c, const char *alpn, uint len) {
                if (!c || !alpn || len == 0) {
                    return Error::invalid_argument;
                }
                INIT_CTX(c->self)
                auto err = external::libssl.SSL_set_alpn_protos_(ssl(c->ssl), reinterpret_cast<const byte *>(alpn), len);
                return err == 0 ? Error::none : Error::internal;
            }

            dll(Error) set_peer_cert(conn::Connection *c, const char *cert) {
                if (!c || !cert) {
                    return Error::invalid_argument;
                }
                INIT_CTX(c->self)
                int err = 0;
                auto &s = external::libssl;
                if (c->mode == client) {
                    err = s.SSL_CTX_load_verify_locations_(sslctx(c->sslctx), cert, nullptr);
                }
                else {
                    auto f = s.SSL_load_client_CA_file_(cert);
                    err = f != nullptr;
                    if (f) {
                        s.SSL_CTX_set_client_CA_list_(sslctx(c->sslctx), f);
                    }
                }
                return err ? Error::none : Error::internal;
            }

            dll(Error) set_self_cert(conn::Connection *c, const char *cert, const char *private_key) {
                if (!c || !cert) {
                    return Error::invalid_argument;
                }
                INIT_CTX(c->self)
                // TODO(on-keyday): load cert file!
                return Error::none;
            }

            dll(Error) start_handshake(conn::Connection *c, mem::CBN<void, const byte *, tsize, bool> send_data) {
                if (!c) {
                    return Error::invalid_argument;
                }
                INIT_CTX(c->self);
                c->tmp_callback = [&](void *varg) {
                    auto arg = static_cast<Args *>(varg);
                    if (arg->type == ArgType::handshake_data) {
                        send_data(arg->data, arg->len, false);
                    }
                };
                auto &s = external::libssl;
                auto err = s.SSL_do_handshake_(ssl(c->ssl));
                auto code = s.SSL_get_error_(ssl(c->ssl), err);
                if (err == 1 || code == SSL_ERROR_WANT_READ) {
                    return Error::none;
                }
                if (external::load_LibCrypto()) {
                    mem::OSSLCB<int, const char *, tsize> cb = [&](const char *s, tsize len) {
                        send_data(reinterpret_cast<const byte *>(s), len, true);
                        return 1;
                    };
                    static_assert(
                        sizeof(mem::any_pointer) == sizeof(void *),
                        "!(hack)!this program runs on platform sizeof(void(*)())==sizeof(void*)");
                    auto ptr = reinterpret_cast<int (*)(const char *, size_t, void *)>(cb.cb);
                    external::libcrypto.ERR_print_errors_cb_(ptr, cb.fctx.ctx);
                }
                return Error::internal;
            }

            Error advance_handshake_core(const byte *data, tsize len, conn::Conn &con, EncryptionLevel level) {
                INIT_CTX(con)
                auto &s = external::libssl;
                // provide crypto data
                auto err = s.SSL_provide_quic_data_(ssl(con->ssl), OSSL_ENCRYPTION_LEVEL(level), data, len);
                if (err != 1) {
                    return Error::internal;
                }
                int alert = 0;
                // advance handshake
                err = s.SSL_do_handshake_(ssl(con->ssl));
                auto code = s.SSL_get_error_(ssl(con->ssl), err);
                if (err == 1 || code == SSL_ERROR_WANT_READ) {
                    return Error::none;
                }
                return Error::internal;
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
