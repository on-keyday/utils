/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/tls/tls.h>
#include <fnet/dll/lazy/ssldll.h>
#include <helper/defer.h>
#include <fnet/dll/glheap.h>
#include <cstring>
#include <fnet/tls/liberr.h>

namespace utils {

    namespace fnet::quic::crypto {
        bool set_quic_method(void* ptr);
    }

    namespace fnet::tls {
        constexpr error::Error errConfigNotInitialized = error::Error("TLSConfig is not initialized. call configure() to initialize.", error::ErrorCategory::sslerr);
        constexpr error::Error errNotInitialized = error::Error("TLS object is not initialized. use create_tls() for initialize object", error::ErrorCategory::validationerr);
        constexpr error::Error errInvalid = error::Error("TLS object has invalid state. maybe library bug!!", error::ErrorCategory::validationerr);
        constexpr error::Error errNoTLS = error::Error("TLS object is not initialized for TLS connection. maybe initialzed for QUIC connection", error::ErrorCategory::validationerr);
        constexpr error::Error errNoQUIC = error::Error("TLS object is not initialized for QUIC connection. maybe initialized for TLS connection", error::ErrorCategory::validationerr);
        constexpr error::Error errSSLNotInitialized = error::Error("TLS object is not set up for connection. call setup_ssl() or setup_quic() before", error::ErrorCategory::validationerr);
        // constexpr error::Error errAlready = error::Error("TLS object is already set up", error::ErrorCategory::sslerr);
        constexpr error::Error errNotSupport = error::Error("not supported", error::ErrorCategory::fneterr);
        constexpr error::Error errLibJudge = error::Error("library type judgement failure. maybe other type library?", error::ErrorCategory::fneterr);

        fnet_dll_implement(bool) load_crypto() {
            return lazy::libcrypto.load();
        }

        fnet_dll_implement(bool) load_ssl() {
            return load_crypto() && lazy::libssl.load();
        }

        fnet_dll_implement(bool) is_open_ssl() {
            return lazy::crypto::ossl::sp::CRYPTO_set_mem_functions_.find();
        }

        fnet_dll_implement(bool) is_boring_ssl() {
            return lazy::crypto::bssl::sp::BIO_should_retry_.find();
        }

        struct SSLContexts {
            ssl_import::SSL* ssl = nullptr;
            ssl_import::BIO* wbio = nullptr;
            void* user = nullptr;
            int (*quic_cb)(void* user, quic::crypto::MethodArgs) = nullptr;

            ~SSLContexts() {
                if (ssl) {
                    lazy::ssl::SSL_free_(ssl);
                }
                if (wbio) {
                    lazy::crypto::BIO_free_all_(wbio);
                }
            }
        };

        TLS::~TLS() {
            delete_glheap(static_cast<SSLContexts*>(opt));
        }

        fnet_dll_implement(std::pair<TLS, error::Error>) create_tls_with_error(const TLSConfig& conf) {
            if (!conf.ctx) {
                return {TLS{}, errConfigNotInitialized};
            }
            TLS tls;
            auto c = new_from_global_heap<SSLContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts), alignof(SSLContexts)));
            if (!c) {
                return {TLS{}, error::memory_exhausted};
            }
            auto w = helper::defer([&] {
                delete_glheap(c);
            });
            auto ssl = lazy::ssl::SSL_new_(static_cast<ssl_import::SSL_CTX*>(conf.ctx));
            if (!ssl) {
                return {TLS{}, error::memory_exhausted};
            }
            auto r = helper::defer([&]() {
                lazy::ssl::SSL_free_(ssl);
            });
            ssl_import::BIO *pass = nullptr, *hold = nullptr;
            lazy::crypto::BIO_new_bio_pair_(&pass, 0, &hold, 0);
            if (!pass || !hold) {
                return {TLS{}, error::memory_exhausted};
            }
            lazy::ssl::SSL_set_bio_(ssl, pass, pass);
            c->ssl = ssl;
            c->wbio = hold;
            tls.opt = c;
            r.cancel();
            w.cancel();
            return {std::move(tls), error::none};
        }

        fnet_dll_export(std::pair<TLS, error::Error>) create_quic_tls_with_error(const TLSConfig& conf, int (*cb)(void*, quic::crypto::MethodArgs), void* user) {
            if (!conf.ctx) {
                return {TLS{}, errConfigNotInitialized};
            }
            if (!cb) {
                return {TLS{}, error::Error("QUIC callback MUST NOT be null", error::ErrorCategory::validationerr)};
            }
            TLS tls;
            auto c = new_from_global_heap<SSLContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContexts), alignof(SSLContexts)));
            if (!c) {
                return {TLS{}, error::memory_exhausted};
            }
            auto w = helper::defer([&] {
                delete_glheap(c);
            });
            auto ssl = lazy::ssl::SSL_new_(static_cast<ssl_import::SSL_CTX*>(conf.ctx));
            if (!ssl) {
                // TODO(on-keyday): other reason exist?
                return {TLS{}, error::memory_exhausted};
            }
            auto r = helper::defer([&] {
                lazy::ssl::SSL_free_(ssl);
            });
            if (!lazy::ssl::SSL_set_ex_data_(ssl, ssl_import::ssl_appdata_index, c)) {
                return {TLS{}, libError("SSL_set_ex_data", "failed to set SSL ex data", ssl, 0)};
            }
            if (!quic::crypto::set_quic_method(ssl)) {
                return {TLS{}, error::Error("library has no QUIC extensions", error::ErrorCategory::sslerr)};
            }
            c->ssl = ssl;
            c->quic_cb = cb;
            c->user = user;
            tls.opt = c;
            r.cancel();
            w.cancel();
            return {std::move(tls), error::none};
        }

        int quic_callback(void* c, const quic::crypto::MethodArgs& args) {
            auto ctx = static_cast<SSLContexts*>(c);
            if (!ctx->quic_cb) {
                return 0;
            }
            return ctx->quic_cb(ctx->user, args);
        }

        TLSCipher make_cipher(const void* c) {
            return TLSCipher{c};
        }

        tls::TLSCipher get_quic_cipher(void* c) {
            auto ctx = static_cast<SSLContexts*>(c);
            return make_cipher(lazy::ssl::SSL_get_current_cipher_(ctx->ssl));
        }

#define EXPAND_VA_ARG(...) __VA_ARGS__ __VA_OPT__(, )

#define CHECK_CTX(ctx, ...)                                    \
    if (!opt) {                                                \
        return {EXPAND_VA_ARG(__VA_ARGS__) errNotInitialized}; \
    }                                                          \
    auto ctx = static_cast<SSLContexts*>(opt);

#define CHECK_TLS(ctx, ...)                                       \
    CHECK_CTX(ctx, __VA_ARGS__)                                   \
    if (!ctx->ssl) {                                              \
        return {EXPAND_VA_ARG(__VA_ARGS__) errSSLNotInitialized}; \
    }

#define CHECK_TLS_CONN(ctx, ...)                      \
    CHECK_TLS(ctx, __VA_ARGS__)                       \
    if (!ctx->wbio) {                                 \
        return {EXPAND_VA_ARG(__VA_ARGS__) errNoTLS}; \
    }

#define CHECK_QUIC_CONN(ctx, ...)                      \
    CHECK_TLS(ctx, __VA_ARGS__)                        \
    if (!ctx->quic_cb) {                               \
        return {EXPAND_VA_ARG(__VA_ARGS__) errNoQUIC}; \
    }

        error::Error TLS::set_alpn(view::rvec alpn) {
            CHECK_TLS(c)
            int res = 1;
            if (is_boring_ssl()) {
                res = lazy::ssl::bssl::SSL_set_alpn_protos_(c->ssl, alpn.data(), alpn.size());
            }
            else if (is_open_ssl()) {
                res = lazy::ssl::ossl::SSL_set_alpn_protos_(c->ssl, alpn.data(), alpn.size());
            }
            else {
                return errLibJudge;
            }
            if (res != 0) {
                return libError("SSL_set_alpn_protos", "failed to set ALPN", c->ssl, res);
            }
            return error::none;
        }

        error::Error TLS::set_verify(int mode, int (*verify_callback)(int, void*)) {
            CHECK_TLS(c)
            lazy::ssl::SSL_set_verify_(c->ssl, mode,
                                       (int (*)(int, ssl_import::X509_STORE_CTX*))(verify_callback));
            return error::none;
        }

        error::Error TLS::set_client_cert_file(const char* cert) {
            CHECK_CTX(c)
            auto ptr = lazy::ssl::SSL_load_client_CA_file_(cert);
            if (!ptr) {
                return libError("SSL_load_client_CA_file", "failed to load client CA file");
            }
            lazy::ssl::SSL_set_client_CA_list_(c->ssl, ptr);
            return error::none;
        }

        error::Error TLS::set_cert_chain(const char* pubkey, const char* prvkey) {
            CHECK_TLS(c)
            auto res = lazy::ssl::SSL_use_certificate_chain_file_(c->ssl, pubkey);
            if (!res) {
                return libError("SSL_use_certificate_chain_file", "failed to load public key (certificate)");
            }
            res = lazy::ssl::SSL_use_PrivateKey_file_(c->ssl, prvkey, ssl_import::SSL_FILETYPE_PEM_);
            if (!res) {
                return libError("SSL_use_PrivateKey_file", "failed to load private key");
            }
            if (!lazy::ssl::SSL_check_private_key_(c->ssl)) {
                return libError("SSL_check_private_key", "private key and public key are not matched or invalid");
            }
            return error::none;
        }

        error::Error TLS::set_hostname(const char* hostname, bool verify) {
            CHECK_TLS(c)
            auto common = [&]() -> error::Error {
                if (!verify) {
                    return error::none;
                }
                lazy::ssl::SSL_set_hostflags_(c->ssl, ssl_import::X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS_);
                if (!lazy::ssl::SSL_set1_host_(c->ssl, hostname)) {
                    return libError("SSL_set1_host", "failed to set host name check", c->ssl, 0);
                }
                return error::none;
            };
            if (lazy::ssl::bssl::sp::SSL_set_tlsext_host_name_.find()) {
                if (!lazy::ssl::bssl::sp::SSL_set_tlsext_host_name_(c->ssl, hostname)) {
                    return libError("SSL_set_tlsext_host_name", "failed to set host name extenstion", c->ssl, 0);
                }
                return common();
            }
            else if (lazy::ssl::SSL_ctrl_.find()) {
                if (!lazy::ssl::SSL_ctrl_(c->ssl, ssl_import::SSL_CTRL_SET_TLSEXT_HOSTNAME_, 0, (void*)hostname)) {
                    return libError("SSL_set_tlsext_host_name", "failed to set host name extenstion", c->ssl, 0);
                }
                return common();
            }
            return errNotSupport;
        }

        error::Error TLS::set_eraly_data_enabled(bool enable) {
            CHECK_TLS(c);
            lazy::ssl::SSL_set_early_data_enabled_(c->ssl, enable ? 1 : 0);
            return error::none;
        }

        bool TLS::get_early_data_accepted() {
            bool res = false;
            [&]() -> error::Error {
                CHECK_TLS(c);
                res = (bool)lazy::ssl::SSL_early_data_accepted_(c->ssl);
                return error::none;
            }();
            return res;
        }

        error::Error TLS::set_quic_eraly_data_context(view::rvec data) {
            CHECK_QUIC_CONN(c);
            if (!lazy::ssl::SSL_set_quic_early_data_context_(c->ssl, data.data(), data.size())) {
                return error::Error("set_erary_data_context failed");
            }
            return error::none;
        }

        std::pair<view::rvec, error::Error> TLS::provide_tls_data(view::rvec data) {
            CHECK_TLS_CONN(c, data)
            auto limited = data.substr(0, (std::numeric_limits<int>::max)());
            auto res = lazy::crypto::BIO_write_(c->wbio, limited.data(), int(limited.size()));
            if (res < 0) {
                if (lazy::crypto::bssl::sp::BIO_should_retry_.find()) {
                    if (lazy::crypto::bssl::sp::BIO_should_retry_(c->wbio)) {
                        return {data, error::block};
                    }
                }
                else if (lazy::crypto::BIO_test_flags_.find()) {
                    if (lazy::crypto::BIO_test_flags_(c->wbio, ssl_import::BIO_FLAGS_SHOULD_RETRY_)) {
                        return {data, error::block};
                    }
                }
                return {data, libError("BIO_write", "BIO operation failed", c->ssl, res)};
            }
            return {data.substr(res), error::none};
        }

        std::pair<view::wvec, error::Error> TLS::receive_tls_data(view::wvec data) {
            CHECK_TLS_CONN(c, {})
            auto limited = data.substr(0, (std::numeric_limits<int>::max)());
            auto res = lazy::crypto::BIO_read_(c->wbio, limited.data(), int(limited.size()));
            if (res < 0) {
                if (lazy::crypto::bssl::sp::BIO_should_retry_.find()) {
                    if (lazy::crypto::bssl::sp::BIO_should_retry_(c->wbio)) {
                        return {{}, error::block};
                    }
                }
                else if (lazy::crypto::BIO_test_flags_.find()) {
                    if (lazy::crypto::BIO_test_flags_(c->wbio, ssl_import::BIO_FLAGS_SHOULD_RETRY_)) {
                        return {{}, error::block};
                    }
                }
                return {{}, libError("BIO_read", "BIO operation failed", c->ssl, res)};
            }
            return {data.substr(0, res), error::none};
        }

        error::Error TLS::provide_quic_data(quic::crypto::EncryptionLevel level, view::rvec data) {
            CHECK_QUIC_CONN(c)
            auto res = lazy::ssl::SSL_provide_quic_data_(c->ssl,
                                                         ssl_import::OSSL_ENCRYPTION_LEVEL(level),
                                                         data.data(), data.size());
            if (!res) {
                return libError("SSL_provide_quic_data", "cannot provide QUIC CRYPTO data to TLS handshake", c->ssl, res);
            }
            return error::none;
        }

        error::Error TLS::progress_quic() {
            CHECK_QUIC_CONN(c)
            auto res = lazy::ssl::SSL_process_quic_post_handshake_(c->ssl);
            if (!res) {
                return libError("SSL_progress_quic_post_handshake", "cannot provide QUIC CRYPTO data to TLS post handshake", c->ssl, res);
            }
            return error::none;
        }

        error::Error TLS::set_quic_transport_params(view::rvec data) {
            CHECK_QUIC_CONN(c)
            auto res = lazy::ssl::SSL_set_quic_transport_params_(c->ssl, data.data(), data.size());
            if (!res) {
                return libError("SSL_quic_transport_params", "cannot provide QUIC transport parameter extension", c->ssl, res);
            }
            return error::none;
        }

        view::rvec TLS::get_peer_quic_transport_params() {
            view::rvec ret;
            [&]() -> error::Error {
                CHECK_QUIC_CONN(c)
                size_t l = 0;
                const byte* data = nullptr;
                lazy::ssl::SSL_get_peer_quic_transport_params_(c->ssl, &data, &l);
                ret = view::rvec(data, l);
                return error::none;
            }();
            return ret;
        }

        std::pair<view::rvec, error::Error> TLS::write(view::rvec data) {
            CHECK_TLS_CONN(c, data)
            auto limited = data.substr(0, (std::numeric_limits<int>::max)());
            auto res = lazy::ssl::SSL_write_(c->ssl, limited.data(), int(limited.size()));
            if (res <= 0) {
                return {data, libError("SSL_write", "failed to write TLS app data or blocking", c->ssl, res)};
            }
            return {data.substr(res), error::none};
        }

        std::pair<view::wvec, error::Error> TLS::read(view::wvec data) {
            CHECK_TLS_CONN(c, {})
            auto limited = data.substr(0, (std::numeric_limits<int>::max)());
            auto res = lazy::ssl::SSL_read_(c->ssl, limited.data(), int(limited.size()));
            if (res <= 0) {
                return {{}, libError("SSL_read", "failed to read TLS app data or blocking", c->ssl, res)};
            }
            return {data.substr(0, res), error::none};
        }

        fnet_dll_implement(bool) isTLSBlock(const error::Error& err) {
            return err == error::block ||
                   (err.category() == error::ErrorCategory::sslerr &&
                    err.errnum() == ssl_import::SSL_ERROR_WANT_READ_);
        }

        error::Error TLS::connect() {
            CHECK_TLS(c)
            auto res = lazy::ssl::SSL_connect_(c->ssl);
            if (res < 0) {
                return libError("SSL_connect", "TLS handshake failed or blocking", c->ssl, res);
            }
            return error::none;
        }

        error::Error TLS::accept() {
            CHECK_TLS(c)
            auto res = lazy::ssl::SSL_accept_(c->ssl);
            if (res < 0) {
                return libError("SSL_accept", "TLS handshake failed or blocking", c->ssl, res);
            }
            return error::none;
        }

        error::Error TLS::shutdown() {
            CHECK_TLS_CONN(c)
            auto res = lazy::ssl::SSL_shutdown_(c->ssl);
            if (res < 0) {
                return libError("SSL_shutdown", "TLS alert failed or blocking", c->ssl, res);
            }
            return error::none;
        }

        bool TLS::has_ssl() const {
            if (!opt) {
                return false;
            }
            return static_cast<SSLContexts*>(opt)->ssl != nullptr;
        }

        error::Error TLS::verify_ok() {
            CHECK_TLS(c)
            auto res = lazy::ssl::SSL_get_verify_result_(c->ssl);
            if (res != ssl_import::X509_V_OK_) {
                return libError("SSL_get_verify_result", "TLS verification result is not ok", c->ssl, res);
            }
            return error::none;
        }

        bool TLS::get_alpn(const char** selected, unsigned int* len) {
            return [&]() -> error::Error {
                CHECK_TLS(c)
                lazy::ssl::SSL_get0_alpn_selected_(c->ssl, reinterpret_cast<const unsigned char**>(selected), len);
                return error::none;
            }()
                                .is_noerr();
        }

        void TLS::get_errors(int (*cb)(const char*, size_t, void*), void* user) {
            if (!cb) {
                return;
            }
            lazy::crypto::ERR_print_errors_cb_(cb, user);
        }

        fnet_dll_implement(void) set_libssl(lazy::dll_path path) {
            lazy::libssl.replace_path(path, false);
        }

        fnet_dll_implement(void) set_libcrypto(lazy::dll_path path) {
            lazy::libcrypto.replace_path(path, false, lazy::libcrypto_init, nullptr);
        }

        const char* TLSCipher::name() const {
            return lazy::ssl::SSL_CIPHER_get_name_(static_cast<const ssl_import::SSL_CIPHER*>(cipher));
        }

        const char* TLSCipher::rfcname() const {
            return cipher ? lazy::ssl::SSL_CIPHER_standard_name_(static_cast<const ssl_import::SSL_CIPHER*>(cipher)) : "(NONE)";
        }

        int TLSCipher::bits(int* bit) const {
            return lazy::ssl::SSL_CIPHER_get_bits_(static_cast<const ssl_import::SSL_CIPHER*>(cipher), bit);
        }

        int TLSCipher::nid() const {
            return lazy::ssl::SSL_CIPHER_get_cipher_nid_(static_cast<const ssl_import::SSL_CIPHER*>(cipher));
        }

        TLSCipher TLS::get_cipher() {
            TLSCipher ciph;
            [&]() -> error::Error {
                CHECK_TLS(c)
                ciph = make_cipher(lazy::ssl::SSL_get_current_cipher_(c->ssl));
                return error::none;
            }();
            return ciph;
        }

        Session TLS::get_session() {
            Session sess;
            [&]() -> error::Error {
                CHECK_TLS(c)
                sess.sess = lazy::ssl::SSL_get_session_(c->ssl);
                return error::none;
            }();
            return sess;
        }

        bool TLS::set_session(Session&& sess) {
            if (!sess.sess) {
                return false;
            }
            bool ok = false;
            [&]() -> error::Error {
                CHECK_TLS(c)
                ok = lazy::ssl::SSL_set_session_(c->ssl, static_cast<ssl_import::SSL_SESSION*>(sess.sess)) == 1;
                return error::none;
            }();
            return ok;
        }
    }  // namespace fnet::tls
}  // namespace utils
