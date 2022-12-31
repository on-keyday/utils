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
#include <helper/defer.h>
#include <dnet/dll/glheap.h>
#include <cstring>

namespace utils {
    namespace dnet {
        dnet_dll_implement(bool) load_crypto() {
            static bool res = ssldl.load_crypto_common();
            return res;
        }

        dnet_dll_implement(bool) load_ssl() {
            static bool res = load_crypto() && ssldl.load_ssl_common();
            return res;
        }

        dnet_dll_implement(bool) is_boringssl_ssl() {
            static bool res = load_ssl() && ssldl.load_boringssl_ssl_ext();
            return res;
        }

        dnet_dll_implement(bool) is_openssl_ssl() {
            static bool res = load_ssl() && ssldl.load_openssl_ssl_ext();
            return res;
        }

        dnet_dll_implement(bool) is_boringssl_crypto() {
            static bool res = load_crypto() && ssldl.load_boringssl_crypto_ext();
            return res;
        }

        dnet_dll_implement(bool) is_openssl_crypto() {
            static bool res = load_crypto() && ssldl.load_openssl_crypto_ext();
            return res;
        }

        struct SSLContexts {
            ssl_import::SSL* ssl = nullptr;
            ssl_import::SSL_CTX* sslctx = nullptr;
            ssl_import::BIO* wbio = nullptr;
            void* user = nullptr;
            int (*cb)(void* user, quic::MethodArgs) = nullptr;

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
            delete_with_global_heap(static_cast<SSLContexts*>(opt), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts), alignof(SSLContexts)));
        }

        dnet_dll_implement(TLS) create_tls() {
            if (!load_ssl()) {
                return {libload_failed};
            }
            auto ctx = ssldl.SSL_CTX_new_(ssldl.TLS_method_());
            if (!ctx) {
                return {no_resource};
            }
            auto d = helper::defer([&] {
                ssldl.SSL_CTX_free_(ctx);
            });
            auto tls = new_from_global_heap<SSLContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts), alignof(SSLContexts)));
            if (!tls) {
                return {no_resource};
            }
            tls->sslctx = ctx;
            d.cancel();
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
            auto new_tls = new_from_global_heap<SSLContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts), alignof(SSLContexts)));
            if (!new_tls) {
                return {no_resource};
            }
            if (!ssldl.SSL_CTX_up_ref_(ctx->sslctx)) {
                delete_with_global_heap(new_tls, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts), alignof(SSLContexts)));
                return {no_resource};
            }
            new_tls->sslctx = ctx->sslctx;
            return {new_tls};
        }

        error::Error check_opt(void* opt, int& err, auto&& callback) {
            if (!opt) {
                return error::Error(invalid_tls, error::ErrorCategory::validationerr);
            }
            auto ctx = static_cast<SSLContexts*>(opt);
            if (!ctx->sslctx) {
                return error::Error(invalid_tls, error::ErrorCategory::validationerr);
            }
            return callback(ctx);
        }

        error::Error check_opt_bio(void* opt, int& err, auto&& callback) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                if (!c->ssl || !c->wbio) {
                    return error::Error(should_setup_ssl, error::ErrorCategory::validationerr);
                }
                return callback(c);
            });
        }

        error::Error check_opt_ssl(void* opt, int& err, auto&& callback) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                if (!c->ssl) {
                    return error::Error(should_setup_ssl, error::ErrorCategory::validationerr);
                }
                return callback(c);
            });
        }

        error::Error check_opt_quic(void* opt, int& err, auto&& callback) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                if (!c->ssl || c->wbio) {
                    error::Error(should_setup_quic, error::ErrorCategory::validationerr);
                }
                return callback(c);
            });
        }

        error::Error TLS::set_alpn(const void* p, size_t len) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_set_alpn_protos_(c->ssl, static_cast<const unsigned char*>(p), len);
                if (res != 0) {
                    return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                }
                return error::none;
            });
        }

        error::Error TLS::set_cacert_file(const char* cacert, const char* dir) {
            return check_opt(opt, err, [&](SSLContexts* c) -> error::Error {
                if (!ssldl.SSL_CTX_load_verify_locations_(c->sslctx, cacert, dir)) {
                    err = set_ssl_value_failed;
                    return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                }
                return error::none;
            });
        }

        error::Error TLS::set_verify(int mode, int (*verify_callback)(int, void*)) {
            return check_opt(opt, err, [&](SSLContexts* c) {
                ssldl.SSL_CTX_set_verify_(c->sslctx, mode,
                                          (int (*)(int, ssl_import::X509_STORE_CTX*))(verify_callback));
                return error::none;
            });
        }

        error::Error TLS::make_ssl() {
            return check_opt(opt, err, [&](SSLContexts* c) -> error::Error {
                if (c->ssl) {
                    return error::Error("already initialized", error::ErrorCategory::validationerr);
                }
                auto ssl = ssldl.SSL_new_(c->sslctx);
                if (!ssl) {
                    return error::memory_exhausted;
                }
                auto r = helper::defer([&]() {
                    ssldl.SSL_free_(ssl);
                });
                if (!load_crypto()) {
                    return error::Error(libload_failed, error::ErrorCategory::dneterr);
                }
                ssl_import::BIO *pass = nullptr, *hold = nullptr;
                ssldl.BIO_new_bio_pair_(&pass, 0, &hold, 0);
                if (!pass || !hold) {
                    return error::memory_exhausted;
                }
                ssldl.SSL_set_bio_(ssl, pass, pass);
                c->ssl = ssl;
                c->wbio = hold;
                r.cancel();
                return error::none;
            });
        }

        int quic_callback(TLS& tls, const quic::MethodArgs& args) {
            int res = 0;
            check_opt_quic(tls.opt, tls.err, [&](SSLContexts* c) {
                if (c->cb) {
                    res = c->cb(c->user, args);
                }
                return error::none;
            });
            return res;
        }

        namespace quic::crypto {
            bool set_quic_method(void* ptr);
        }

        error::Error TLS::make_quic(int (*cb)(void*, quic::MethodArgs), void* user) {
            return check_opt(opt, err, [&](SSLContexts* c) -> error::Error {
                if (c->ssl) {
                    return error::Error("already initialized", error::ErrorCategory::dneterr);
                }
                auto ssl = ssldl.SSL_new_(c->sslctx);
                if (!ssl) {
                    return error::memory_exhausted;
                }
                auto r = helper::defer([&]() {
                    ssldl.SSL_free_(ssl);
                });
                if (!ssldl.SSL_set_ex_data_(ssl, ssl_appdata_index, this)) {
                    return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                }
                if (!quic::crypto::set_quic_method(ssl)) {
                    err = libload_failed;
                    return error::Error(libload_failed, error::ErrorCategory::dneterr);
                }
                c->ssl = ssl;
                c->cb = cb;
                c->user = user;
                r.cancel();
                return error::none;
            });
        }

        error::Error TLS::set_hostname(const char* hostname, bool verify) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) {
                auto common = [&]() -> error::Error {
                    if (!verify) {
                        return error::none;
                    }
                    ssldl.SSL_set_hostflags_(c->ssl, ssl_import::X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS_);
                    if (!ssldl.SSL_set1_host_(c->ssl, hostname)) {
                        return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                    }
                    return error::none;
                };
                if (is_boringssl_ssl()) {
                    if (!ssldl.SSL_set_tlsext_host_name_(c->ssl, hostname)) {
                        return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                    }
                    return common();
                }
                else if (is_openssl_ssl()) {
                    if (!ssldl.SSL_ctrl_(c->ssl, ssl_import::SSL_CTRL_SET_TLSEXT_HOSTNAME_, 0, (void*)hostname)) {
                        return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                    }
                    return common();
                }
                return error::Error(not_supported, error::ErrorCategory::dneterr);
            });
        }

        error::Error TLS::set_client_cert_file(const char* cert) {
            return check_opt(opt, err, [&](SSLContexts* c) -> error::Error {
                auto ptr = ssldl.SSL_load_client_CA_file_(cert);
                if (!ptr) {
                    return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                }
                ssldl.SSL_CTX_set_client_CA_list_(c->sslctx, ptr);
                return error::none;
            });
        }

        error::Error TLS::set_cert_chain(const char* pubkey, const char* prvkey) {
            return check_opt(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_CTX_use_certificate_chain_file_(c->sslctx, pubkey);
                if (!res) {
                    return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                }
                res = ssldl.SSL_CTX_use_PrivateKey_file_(c->sslctx, prvkey, ssl_import::SSL_FILETYPE_PEM_);
                if (!res) {
                    return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                }
                if (!ssldl.SSL_CTX_check_private_key_(c->sslctx)) {
                    return error::Error(set_ssl_value_failed, error::ErrorCategory::dneterr);
                }
                return error::none;
            });
        }

        error::Error TLS::provide_tls_data(const void* data, size_t len, size_t* written) {
            return check_opt_bio(opt, err, [&](SSLContexts* c) -> error::Error {
                if (is_openssl_crypto()) {
                    size_t tmp = 0;
                    if (!written) {
                        written = &tmp;
                    }
                    if (!ssldl.BIO_write_ex_(c->wbio, data, len, written)) {
                        if (ssldl.BIO_test_flags_(c->wbio, ssl_import::BIO_FLAGS_SHOULD_RETRY_)) {
                            return error::block;
                        }
                        return error::Error(bio_operation_failed, error::ErrorCategory::dneterr);
                    }
                    return error::none;
                }
                else if (is_boringssl_crypto()) {
                    auto res = ssldl.BIO_write_(c->wbio, data, int(len));
                    if (res < 0) {
                        if (ssldl.BIO_should_retry_(c->wbio)) {
                            return error::block;
                        }
                        return error::Error(bio_operation_failed, error::ErrorCategory::dneterr);
                    }
                    if (written) {
                        *written = res;
                    }
                    return error::none;
                }
                return error::Error(not_supported, error::ErrorCategory::dneterr);
            });
        }

        error::Error TLS::receive_tls_data(void* data, size_t len) {
            auto& red = prevred;
            return check_opt_bio(opt, err, [&](SSLContexts* c) -> error::Error {
                if (is_openssl_crypto()) {
                    if (!ssldl.BIO_read_ex_(c->wbio, data, len, &red)) {
                        if (ssldl.BIO_test_flags_(c->wbio, ssl_import::BIO_FLAGS_SHOULD_RETRY_)) {
                            return error::block;
                        }
                        return error::Error(bio_operation_failed, error::ErrorCategory::dneterr);
                    }
                    return error::none;
                }
                else if (is_boringssl_crypto()) {
                    auto res = ssldl.BIO_read_(c->wbio, data, int(len));
                    if (res < 0) {
                        if (ssldl.BIO_should_retry_(c->wbio)) {
                            return error::block;
                        }
                        return error::Error(bio_operation_failed, error::ErrorCategory::dneterr);
                    }
                    red = res;
                    return error::none;
                }
                return error::Error(not_supported, error::ErrorCategory::dneterr);
            });
        }

        error::Error TLS::provide_quic_data(quic::crypto::EncryptionLevel level, const void* data, size_t size) {
            return check_opt_quic(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_provide_quic_data_(c->ssl,
                                                        ssl_import::quic_ext::OSSL_ENCRYPTION_LEVEL(level),
                                                        reinterpret_cast<const uint8_t*>(data), size);
                if (!res) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, 0), error::ErrorCategory::sslerr);
                }
                return error::none;
            });
        }

        error::Error TLS::progress_quic() {
            return check_opt_quic(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_process_quic_post_handshake_(c->ssl);
                if (!res) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, 0), error::ErrorCategory::sslerr);
                }
                return error::none;
            });
        }

        error::Error TLS::set_quic_transport_params(const void* params, size_t len) {
            return check_opt_quic(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_set_quic_transport_params_(c->ssl, static_cast<const uint8_t*>(params), len);
                if (!res) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, 0), error::ErrorCategory::sslerr);
                }
                return error::none;
            });
        }

        const byte* TLS::get_peer_quic_transport_params(size_t* len) {
            const byte* data = nullptr;
            size_t l = 0;
            check_opt_quic(opt, err, [&](SSLContexts* c) {
                ssldl.SSL_get_peer_quic_transport_params_(c->ssl, &data, &l);
                return error::none;
            });
            if (len) {
                *len = l;
            }
            return data;
        }

        error::Error TLS::write(const void* data, size_t len, size_t* written) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_write_(c->ssl, data, int(len));
                if (res <= 0) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, res), error::ErrorCategory::sslerr);
                }
                else if (written) {
                    *written = res;
                }
                return error::none;
            });
        }

        error::Error TLS::read(void* data, size_t len) {
            auto& red = prevred;
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_read_(c->ssl, data, int(len));
                if (res <= 0) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, res), error::ErrorCategory::sslerr);
                }
                else {
                    red = res;
                }
                return error::none;
            });
        }

        dnet_dll_implement(bool) isTLSBlock(const error::Error& err) {
            return err == error::block ||
                   (err.category() == error::ErrorCategory::sslerr &&
                    err.errnum() == ssl_import::SSL_ERROR_WANT_READ_);
        }

        bool TLS::closed() const {
            return err == ssl_import::SSL_ERROR_ZERO_RETURNED_;
        }

        error::Error TLS::connect() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_connect_(c->ssl);
                if (res < 0) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, res), error::ErrorCategory::sslerr);
                }
                return error::none;
            });
        }

        error::Error TLS::accept() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_accept_(c->ssl);
                if (res < 0) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, res), error::ErrorCategory::sslerr);
                }
                return error::none;
            });
        }

        error::Error TLS::shutdown() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_shutdown_(c->ssl);
                if (res < 0) {
                    return error::Error(ssldl.SSL_get_error_(c->ssl, res), error::ErrorCategory::sslerr);
                }
                return error::none;
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

        error::Error TLS::verify_ok() {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                auto res = ssldl.SSL_get_verify_result_(c->ssl);
                if (res != ssl_import::X509_V_OK_) {
                    return error::Error(res, error::ErrorCategory::cryptoerr);
                }
                return error::none;
            });
        }

        bool TLS::get_alpn(const char** selected, unsigned int* len) {
            return check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                       ssldl.SSL_get0_alpn_selected_(c->ssl, reinterpret_cast<const unsigned char**>(selected), len);
                       return error::none;
                   })
                .is_noerr();
        }

        void TLS::get_errors(int (*cb)(const char*, size_t, void*), void* user) {
            if (!cb) {
                return;
            }
            ssldl.ERR_print_errors_cb_(cb, user);
        }

        dnet_dll_implement(void) set_libssl(const char* path) {
            ssldl.set_libssl(path);
        }

        dnet_dll_implement(void) set_libcrypto(const char* path) {
            ssldl.set_libcrypto(path);
        }

        const char* TLSCipher::name() const {
            return ssldl.SSL_CIPHER_get_name_(static_cast<const ssl_import::SSL_CIPHER*>(cipher));
        }

        const char* TLSCipher::rfcname() const {
            return cipher ? ssldl.SSL_CIPHER_standard_name_(static_cast<const ssl_import::SSL_CIPHER*>(cipher)) : "(NONE)";
        }

        int TLSCipher::bits(int* bit) const {
            return ssldl.SSL_CIPHER_get_bits_(static_cast<const ssl_import::SSL_CIPHER*>(cipher), bit);
        }

        int TLSCipher::nid() const {
            return ssldl.SSL_CIPHER_get_cipher_nid_(static_cast<const ssl_import::SSL_CIPHER*>(cipher));
        }

        TLSCipher make_cipher(const void* c) {
            return TLSCipher{c};
        }

        TLSCipher TLS::get_cipher() {
            TLSCipher ciph;
            check_opt_ssl(opt, err, [&](SSLContexts* c) -> error::Error {
                ciph = make_cipher(ssldl.SSL_get_current_cipher_(c->ssl));
                return error::none;
            });
            return ciph;
        }
    }  // namespace dnet
}  // namespace utils
