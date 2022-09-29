/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/tls.h>
#include <dnet/dll/ssldll.h>
#include <dnet/errcode.h>
#include <quic/mem/raii.h>
#include <dnet/dll/glheap.h>
#include <cstring>

namespace utils {
    namespace dnet {
        dnet_dll_internal(bool) load_crypto() {
            static bool res = ssldl.load_crypto_common();
            return res;
        }

        dnet_dll_internal(bool) load_ssl() {
            static bool res = load_crypto() && ssldl.load_ssl_common();
            return res;
        }

        bool is_boringssl_ssl() {
            static bool res = load_ssl() && ssldl.load_boringssl_ssl_ext();
            return res;
        }

        bool is_openssl_ssl() {
            static bool res = load_ssl() && ssldl.load_openssl_ssl_ext();
            return res;
        }

        bool is_boringssl_crypto() {
            static bool res = load_crypto() && ssldl.load_boringssl_crypto_ext();
            return res;
        }

        bool is_openssl_crypto() {
            static bool res = load_crypto() && ssldl.load_openssl_crypto_ext();
            return res;
        }

        struct SSLContexts {
            ssl_import::SSL* ssl = nullptr;
            ssl_import::SSL_CTX* sslctx = nullptr;
            ssl_import::BIO* wbio = nullptr;

            ~SSLContexts() {
                if (ssl) {
                    ssldl.SSL_free_(ssl);
                }
                if (sslctx) {
                    ssldl.SSL_CTX_free_(sslctx);
                }
                if (wbio) {
                    ssldl.BIO_free_all_(wbio);
                }
            }
        };

        TLS::~TLS() {
            delete_with_global_heap(static_cast<SSLContexts*>(opt), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts)));
        }

        dnet_dll_internal(TLS) create_tls() {
            if (!load_ssl()) {
                return {libload_failed};
            }
            auto ctx = ssldl.SSL_CTX_new_(ssldl.TLS_method_());
            if (!ctx) {
                return {no_resource};
            }
            quic::mem::RAII rctx{
                ctx,
                [](auto ctx) {
                    if (ctx) {
                        ssldl.SSL_CTX_free_(ctx);
                    }
                },
            };
            auto tls = new_from_global_heap<SSLContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts)));
            if (!tls) {
                return {no_resource};
            }
            tls->sslctx = ctx;
            rctx.t = nullptr;
            return {tls};
        }

        dnet_dll_export(TLS) create_tls_from(const TLS& tls) {
            if (!load_ssl()) {
                return {no_resource};
            }
            if (!tls.opt) {
                return {invalid_tls};
            }
            auto ctx = static_cast<SSLContexts*>(tls.opt);
            if (!ctx->sslctx) {
                return {invalid_tls};
            }
            auto new_tls = new_from_global_heap<SSLContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts)));
            if (!new_tls) {
                return {no_resource};
            }
            if (!ssldl.SSL_CTX_up_ref_(ctx->sslctx)) {
                delete_with_global_heap(new_tls, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts)));
                return {no_resource};
            }
            new_tls->sslctx = ctx->sslctx;
            return {new_tls};
        }

        bool check_opt(void* opt, int& err, auto&& callback) {
            if (!opt) {
                err = invalid_tls;
                return false;
            }
            auto ctx = static_cast<SSLContexts*>(opt);
            if (!ctx->sslctx) {
                err = invalid_tls;
                return false;
            }
            return callback(ctx);
        }

        bool check_opt_bio(void* opt, int& err, auto&& callback) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                if (!c->ssl || !c->wbio) {
                    err = should_setup_ssl;
                    return false;
                }
                return callback(c);
            });
        }

        bool check_opt_ssl(void* opt, int& err, auto&& callback) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                if (!c->ssl) {
                    err = should_setup_ssl;
                    return false;
                }
                return callback(c);
            });
        }

        bool TLS::set_alpn(const void* p, size_t len) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_set_alpn_protos_(c->ssl, static_cast<const unsigned char*>(p), len);
                if (res != 0) {
                    err = set_ssl_value_failed;
                }
                return res == 0;
            });
        }

        bool TLS::set_cacert_file(const char* cacert, const char* dir) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                if (!ssldl.SSL_CTX_load_verify_locations_(c->sslctx, cacert, dir)) {
                    err = set_ssl_value_failed;
                    return false;
                }
                return true;
            });
        }

        bool TLS::make_ssl() {
            return check_opt(opt, err, [&](SSLContexts* c) {
                if (c->ssl) {
                    return true;
                }
                auto ssl = ssldl.SSL_new_(c->sslctx);
                if (!ssl) {
                    err = no_resource;
                    return false;
                }
                quic::mem::RAII r{
                    ssl, [](auto ssl) {
                        ssldl.SSL_free_(ssl);
                    }};
                if (!load_crypto()) {
                    err = libload_failed;
                    return false;
                }
                ssl_import::BIO *pass = nullptr, *hold = nullptr;
                ssldl.BIO_new_bio_pair_(&pass, 0, &hold, 0);
                if (!pass || !hold) {
                    err = no_resource;
                    return false;
                }
                ssldl.SSL_set_bio_(ssl, pass, pass);
                c->ssl = ssl;
                r.t = nullptr;
                c->wbio = hold;
                return true;
            });
        }

        bool TLS::set_hostname(const char* hostname) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto common = [&]() -> bool {
                    if (!ssldl.SSL_set1_host_(c->ssl, hostname)) {
                        err = set_ssl_value_failed;
                        return false;
                    }
                    return true;
                };
                if (is_boringssl_ssl()) {
                    if (!ssldl.SSL_set_tlsext_host_name_(c->ssl, hostname)) {
                        err = set_ssl_value_failed;
                        return false;
                    }
                    return common();
                }
                else if (is_openssl_ssl()) {
                    if (!ssldl.SSL_ctrl_(c->ssl, ssl_import::SSL_CTRL_SET_TLSEXT_HOSTNAME_, 0, (void*)hostname)) {
                        err = set_ssl_value_failed;
                        return false;
                    }
                    return common();
                }
                err = not_supported;
                return false;
            });
        }

        bool TLS::set_client_cert_file(const char* cert) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                auto ptr = ssldl.SSL_load_client_CA_file_(cert);
                if (!ptr) {
                    return false;
                }
                ssldl.SSL_CTX_set_client_CA_list_(c->sslctx, ptr);
                return true;
            });
        }

        bool TLS::set_cert_chain(const char* pubkey, const char* prvkey) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_CTX_use_certificate_chain_file_(c->sslctx, pubkey);
                if (!res) {
                    err = set_ssl_value_failed;
                    return false;
                }
                res = ssldl.SSL_CTX_use_PrivateKey_file_(c->sslctx, prvkey, ssl_import::SSL_FILETYPE_PEM_);
                if (!res) {
                    err = set_ssl_value_failed;
                    return false;
                }
                if (!ssldl.SSL_CTX_check_private_key_(c->sslctx)) {
                    err = set_ssl_value_failed;
                    return false;
                }
                return true;
            });
        }

        bool TLS::provide_tls_data(const void* data, size_t len, size_t* written) {
            return check_opt_bio(opt, err, [&](SSLContexts* c) {
                if (is_openssl_crypto()) {
                    size_t tmp = 0;
                    if (!written) {
                        written = &tmp;
                    }
                    if (!ssldl.BIO_write_ex_(c->wbio, data, len, written)) {
                        if (ssldl.BIO_test_flags_(c->wbio, ssl_import::BIO_FLAGS_SHOULD_RETRY_)) {
                            err = ssl_blocking;
                            return false;
                        }

                        err = bio_operation_failed;
                        return false;
                    }
                    return true;
                }
                else if (is_boringssl_crypto()) {
                    auto res = ssldl.BIO_write_(c->wbio, data, int(len));
                    if (res < 0) {
                        if (ssldl.BIO_should_retry_(c->wbio)) {
                            err = ssl_blocking;
                            return false;
                        }
                        err = bio_operation_failed;
                        return false;
                    }
                    if (written) {
                        *written = res;
                    }
                    return true;
                }
                err = not_supported;
                return false;
            });
        }

        bool TLS::receive_tls_data(void* data, size_t len) {
            auto& red = prevred;
            return check_opt_bio(opt, err, [&](SSLContexts* c) {
                if (is_openssl_crypto()) {
                    if (!ssldl.BIO_read_ex_(c->wbio, data, len, &red)) {
                        if (ssldl.BIO_test_flags_(c->wbio, ssl_import::BIO_FLAGS_SHOULD_RETRY_)) {
                            err = ssl_blocking;
                            return false;
                        }
                        err = bio_operation_failed;
                        return false;
                    }
                    return true;
                }
                else if (is_boringssl_crypto()) {
                    auto res = ssldl.BIO_read_(c->wbio, data, int(len));
                    if (res < 0) {
                        if (ssldl.BIO_should_retry_(c->wbio)) {
                            err = ssl_blocking;
                            return false;
                        }
                        err = bio_operation_failed;
                        return false;
                    }
                    red = res;
                    return true;
                }
                err = not_supported;
                return false;
            });
        }

        bool TLS::write(const void* data, size_t len, size_t* written) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_write_(c->ssl, data, int(len));
                if (res < 0) {
                    err = ssldl.SSL_get_error_(c->ssl, res);
                }
                else if (written) {
                    *written = res;
                }
                return res >= 0;
            });
        }

        bool TLS::read(void* data, size_t len) {
            auto& red = prevred;
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_read_(c->ssl, data, int(len));
                if (res <= 0) {
                    err = ssldl.SSL_get_error_(c->ssl, res);
                }
                else {
                    red = res;
                }
                return res > 0;
            });
        }

        bool TLS::block() const {
            return err == ssl_blocking || err == ssl_import::SSL_ERROR_WANT_READ_;
        }

        bool TLS::closed() const {
            return err == ssl_import::SSL_ERROR_ZERO_RETURNED_;
        }

        bool TLS::connect() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_connect_(c->ssl);
                if (res < 0) {
                    err = ssldl.SSL_get_error_(c->ssl, res);
                }
                return res >= 0;
            });
        }
        bool TLS::accept() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_accept_(c->ssl);
                if (res < 0) {
                    err = ssldl.SSL_get_error_(c->ssl, res);
                }
                return res >= 0;
            });
        }

        bool TLS::shutdown() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_shutdown_(c->ssl);
                if (res < 0) {
                    err = ssldl.SSL_get_error_(c->ssl, res);
                }
                return res >= 0;
            });
        }

        bool TLS::has_ssl() const {
            if (!opt) {
                return false;
            }
            return static_cast<SSLContexts*>(opt)->ssl != nullptr;
        }

        bool TLS::has_sslctx() const {
            if (!opt) {
                return false;
            }
            return static_cast<SSLContexts*>(opt)->sslctx != nullptr;
        }

        bool TLS::progress_state(TLSIO& state, void* iobuf, size_t len) {
            if (!iobuf || !len) {
                err = invalid_argument;
                return false;
            }
            if (state.state == TLSIOState::from_ssl) {
                memset(iobuf, 0, len);
                if (!receive_tls_data(iobuf, len)) {
                    if (!block()) {
                        state.state = TLSIOState::fatal;
                    }
                    return false;
                }
                state.state = TLSIOState::to_ssl;
                return true;
            }
            if (state.state == TLSIOState::to_ssl) {
                if (!provide_tls_data(iobuf, len)) {
                    if (!block()) {
                        state.state = TLSIOState::fatal;
                    }
                    return false;
                }
                state.state = TLSIOState::from_ssl;
                return true;
            }
            return state.state != TLSIOState::fatal;
        }

        bool TLS::verify_ok() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto res = ssldl.SSL_get_verify_result_(c->ssl);
                if (res != ssl_import::X509_V_OK_) {
                    err = res;
                    return false;
                }
                return true;
            });
        }

        bool TLS::get_alpn(const char** selected, unsigned int* len) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                ssldl.SSL_get0_alpn_selected_(c->ssl, reinterpret_cast<const unsigned char**>(selected), len);
                return true;
            });
        }

        void TLS::get_errors(int (*cb)(const char*, size_t, void*), void* user) {
            if (!cb) {
                return;
            }
            ssldl.ERR_print_errors_cb_(cb, user);
        }

        dnet_dll_internal(void) set_libssl(const char* path) {
            ssldl.set_libssl(path);
        }

        dnet_dll_internal(void) set_libcrypto(const char* path) {
            ssldl.set_libcrypto(path);
        }
    }  // namespace dnet
}  // namespace utils
