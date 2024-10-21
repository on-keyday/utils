/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/dll/lazy/ssldll.h>
#include <fnet/dll/lazy/load_error.h>
#include <fnet/tls/tls.h>
#include <fnet/tls/liberr.h>
#include <fnet/dll/errno.h>

namespace futils {
    namespace fnet::tls {

        // this call both SSL_get_error and ERR_get_error
        // see also https://stackoverflow.com/questions/18179128/how-to-manage-the-error-queue-in-openssl-ssl-get-error-and-err-get-error
        error::Error libError(const char* method, const char* msg, void* ssl, int res) {
            LibError err;
            err.method = method;
            auto reterr = [&]() -> error::Error {
                if (err.ssl_code == ssl_import::SSL_ERROR_WANT_READ_) {
                    return error::block;  // temporary for compatibility
                }
                return err;
            };
            if (ssl) {
                auto s = static_cast<ssl_import::SSL*>(ssl);
                err.ssl_code = lazy::ssl::SSL_get_error_(s, res);
            }
            bool cant_load = false;
            auto get_error_code = [&]() -> std::uint32_t {
                if (is_boring_ssl()) {  // boring ssl
                    return lazy::crypto::bssl::ERR_get_error_();
                }
                else if (is_open_ssl()) {  // open ssl
                    return lazy::crypto::ossl::ERR_get_error_();
                }
                cant_load = true;
                return 0;
            };
            err.err_code = get_error_code();
            if (cant_load) {
                err.alt_err = error::Error("cannot decide OpenSSL/BoringSSL. maybe different type library?", error::Category::lib, error::fnet_tls_lib_type_error);
                return err;
            }
            if (err.err_code == 0) {
                return reterr();
            }
            LibSubError* sub = nullptr;
            for (;;) {
                auto next_err = get_error_code();
                if (next_err == 0) {
                    break;
                }
                if (!sub) {
                    err.alt_err = LibSubError{};
                    sub = err.alt_err.as<LibSubError>();
                    sub->code = next_err;
                }
                else {
                    sub->alt_err = LibSubError{};
                    sub = sub->alt_err.as<LibSubError>();
                    sub->code = next_err;
                }
            }
            return reterr();
        }

        void print_lib_error(helper::IPushBacker<> pb, const char* name, auto code) {
            strutil::appends(pb, name, "=error:");
            number::to_string(pb, code, 16);
            strutil::append(pb, ":");
            if (is_boring_ssl()) {
                auto str = lazy::crypto::bssl::ERR_lib_error_string_(code);
                if (str) {
                    strutil::append(pb, str);
                }
                strutil::append(pb, "::");
                str = lazy::crypto::bssl::ERR_reason_error_string_(code);
                if (str) {
                    strutil::append(pb, str);
                }
            }
            else if (is_open_ssl()) {
                auto str = lazy::crypto::ossl::ERR_lib_error_string_(code);
                if (str) {
                    strutil::append(pb, str);
                }
                strutil::append(pb, "::");
                str = lazy::crypto::ossl::ERR_reason_error_string_(code);
                if (str) {
                    strutil::append(pb, str);
                }
            }
            else {
                strutil::append(pb, "::");
            }
        }

        void LibError::error(helper::IPushBacker<> pb) const {
            if (method) {
                strutil::appends(pb, "method=\"", method, "\" ");
            }
            if (ssl_code != 0) {
                strutil::append(pb, "ssl_error_code=");
                number::to_string(pb, ssl_code);
                if (lazy::ssl::bssl::sp::SSL_error_description_.find()) {  // on boringssl
                    strutil::append(pb, " ");
                    auto str = lazy::ssl::bssl::sp::SSL_error_description_(ssl_code);
                    if (str) {
                        strutil::appends(pb, "ssl_error_desc=\"", str, "\" ");
                    }
                }
            }
            if (err_code != 0) {
                if (ssl_code != 0) {
                    strutil::append(pb, " ");
                }
                print_lib_error(pb, "lib_error", err_code);
            }
            if (alt_err) {
                strutil::append(pb, " ");
                alt_err.error(pb);
            }
        }

        void LibSubError::error(helper::IPushBacker<> pb) const {
            print_lib_error(pb, "sub_error", code);
            if (alt_err) {
                strutil::append(pb, " ");
                alt_err.error(pb);
            }
        }

        struct SSLContextHolder {
            ssl_import::SSL_CTX* ctx = nullptr;
            bool (*alpn_callback)(ALPNSelector& sel, void*) = nullptr;
            void* alpn_callback_arg = nullptr;
            bool (*session_callback)(Session&& sess, void*) = nullptr;
            void* sess_callback_arg = nullptr;
            void (*keylog_callback)(view::rvec line, void*) = nullptr;
            void* keylog_callback_arg = nullptr;
        };

        fnet_dll_implement(expected<TLSConfig>) configure_with_error() {
            TLSConfig conf;
            set_error(0);
            if (!load_ssl()) {
                return unexpect(lazy::load_error("SSL library not loaded"));
            }
            auto ctx = lazy::ssl::SSL_CTX_new_(lazy::ssl::TLS_method_());
            if (!ctx) {
                return unexpect(error::Error("failed to create SSL_CTX", error::Category::lib, error::fnet_tls_error));
            }
            auto release = helper::defer([&] {
                lazy::ssl::SSL_CTX_free_(ctx);
            });
            auto data = new_from_global_heap<SSLContextHolder>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(SSLContextHolder), alignof(SSLContextHolder)));
            data->ctx = ctx;
            int idx = -1;
            auto del_fn = [](void* parent, void* ptr, ssl_import::CRYPTO_EX_DATA* ad,
                             int index, long argl, void* argp) {
                if (!ptr) {
                    return;
                }
                delete_glheap(static_cast<SSLContextHolder*>(ptr));
            };
            auto release_data = helper::defer([&] {
                delete_glheap(static_cast<SSLContextHolder*>(data));
            });
            if (lazy::ssl::bssl::sp::SSL_CTX_get_ex_new_index_.find()) {
                idx = lazy::ssl::bssl::sp::SSL_CTX_get_ex_new_index_(
                    0, nullptr, nullptr, nullptr,
                    +del_fn);
            }
            else if (lazy::crypto::ossl::sp::CRYPTO_get_ex_new_index_.find()) {
                idx = lazy::crypto::ossl::sp::CRYPTO_get_ex_new_index_(
                    ssl_import::CRYPTO_EX_INDEX_SSL_CTX_, 0, nullptr, nullptr, nullptr,
                    +del_fn);
            }
            if (idx < 0) {
                return unexpect(error::Error("failed to get ex data index", error::Category::lib, error::fnet_tls_error));
            }
            if (!lazy::ssl::SSL_CTX_set_ex_data_(ctx, idx, data)) {
                return unexpect(error::Error("failed to set ex data", error::Category::lib, error::fnet_tls_error));
            }
            release_data.cancel();
            if (!lazy::ssl::SSL_CTX_set_ex_data_(ctx, ssl_import::ssl_appdata_index, data)) {
                return unexpect(error::Error("failed to set ex data", error::Category::lib, error::fnet_tls_error));
            }
            release.cancel();
            conf.ctx = ctx;
            return conf;
        }

        TLSConfig::TLSConfig(const TLSConfig& conf) {
            if (conf.ctx) {
                lazy::ssl::SSL_CTX_up_ref_(static_cast<ssl_import::SSL_CTX*>(conf.ctx));
                this->ctx = conf.ctx;
            }
        }

        TLSConfig& TLSConfig::operator=(const TLSConfig& conf) {
            if (this->ctx == conf.ctx) {
                return *this;
            }
            this->~TLSConfig();
            lazy::ssl::SSL_CTX_up_ref_(static_cast<ssl_import::SSL_CTX*>(conf.ctx));
            this->ctx = conf.ctx;
            return *this;
        }

        constexpr auto errNotInitialized = error::Error("TLSConfig is not initialized. call configure() to initialize.", error::Category::lib, error::fnet_tls_usage_error);
        constexpr auto errLibJudge = error::Error("library type judgement failure. maybe other type library?", error::Category::lib, error::fnet_tls_lib_type_error);

#define CHECK_CTX(c)                        \
    if (!ctx) {                             \
        return unexpect(errNotInitialized); \
    }                                       \
    ssl_import::SSL_CTX* c = static_cast<ssl_import::SSL_CTX*>(ctx);

        expected<void> TLSConfig::set_cacert_file(const char* cacert, const char* dir) {
            CHECK_CTX(c)
            if (!lazy::ssl::SSL_CTX_load_verify_locations_(c, cacert, dir)) {
                return unexpect(libError("SSL_CTX_load_verify_locations", "failed to load CA file or CA file directory location"));
            }
            return {};
        }

        int map_verify_mode(VerifyMode);

        expected<void> TLSConfig::set_verify(VerifyMode mode, int (*verify_callback)(int, void*)) {
            CHECK_CTX(c)
            lazy::ssl::SSL_CTX_set_verify_(c, map_verify_mode(mode),
                                           (int (*)(int, ssl_import::X509_STORE_CTX*))(verify_callback));
            return {};
        }

        expected<void> TLSConfig::set_client_cert_file(const char* cert) {
            CHECK_CTX(c)
            auto ptr = lazy::ssl::SSL_load_client_CA_file_(cert);
            if (!ptr) {
                return unexpect(libError("SSL_load_client_CA_file", "failed to load client CA file"));
            }
            lazy::ssl::SSL_CTX_set_client_CA_list_(c, ptr);
            return {};
        }

        expected<void> TLSConfig::set_cert_chain(const char* pubkey, const char* prvkey) {
            CHECK_CTX(c)
            auto res = lazy::ssl::SSL_CTX_use_certificate_chain_file_(c, pubkey);
            if (!res) {
                return unexpect(libError("SSL_CTX_use_certificate_chain_file", "failed to load public key (certificate)"));
            }
            res = lazy::ssl::SSL_CTX_use_PrivateKey_file_(c, prvkey, ssl_import::SSL_FILETYPE_PEM_);
            if (!res) {
                return unexpect(libError("SSL_CTX_use_PrivateKey_file", "failed to load private key"));
            }
            if (!lazy::ssl::SSL_CTX_check_private_key_(c)) {
                return unexpect(libError("SSL_CTX_check_private_key", "private key and public key are not matched or invalid"));
            }
            return {};
        }

        expected<void> TLSConfig::set_alpn(view::rvec alpn) {
            CHECK_CTX(c)
            int res = 0;
            if (is_boring_ssl()) {
                res = lazy::ssl::bssl::SSL_CTX_set_alpn_protos_(c, alpn.data(), alpn.size());
            }
            else if (is_open_ssl()) {
                res = lazy::ssl::ossl::SSL_CTX_set_alpn_protos_(c, alpn.data(), alpn.size());
            }
            else {
                return unexpect(errLibJudge);
            }
            if (res != 0) {
                return unexpect(libError("SSL_CTX_set_alpn_protos", "failed to set ALPN", nullptr, res));
            }
            return {};
        }

        expected<void> TLSConfig::set_early_data_enabled(bool enable) {
            CHECK_CTX(c);
            lazy::ssl::SSL_CTX_set_early_data_enabled_(c, enable ? 1 : 0);
            return {};
        }

        expected<void> TLSConfig::set_alpn_select_callback(bool (*select)(ALPNSelector& sel, void* arg), void* arg) {
            if (!select) {
                return unexpect(error::Error("ALPN select callback is nullptr", error::Category::lib, error::fnet_tls_usage_error));
            }
            CHECK_CTX(c)
            auto holder = static_cast<SSLContextHolder*>(lazy::ssl::SSL_CTX_get_ex_data_(c, ssl_import::ssl_appdata_index));
            assert(c == holder->ctx);
            holder->alpn_callback = select;
            holder->alpn_callback_arg = arg;
            lazy::ssl::SSL_CTX_set_alpn_select_cb_(
                c, [](ssl_import::SSL* ssl, const uint8_t** out, uint8_t* out_len, const uint8_t* in, unsigned int in_len, void* arg) {
                    auto ctx = lazy::ssl::SSL_get_SSL_CTX_(ssl);
                    auto holder = static_cast<SSLContextHolder*>(lazy::ssl::SSL_CTX_get_ex_data_(ctx, ssl_import::ssl_appdata_index));
                    assert(ctx == holder->ctx);
                    ALPNSelector sel{in, in_len, out, out_len};
                    if (!holder->alpn_callback) {
                        return ssl_import::SSL_TLSEXT_ERR_NOACK_;
                    }
                    if (holder->alpn_callback(sel, holder->alpn_callback_arg)) {
                        if (*out) {
                            return ssl_import::SSL_TLSEXT_ERR_OK_;
                        }
                        return ssl_import::SSL_TLSEXT_ERR_NOACK_;
                    }
                    else {
                        return ssl_import::SSL_TLSEXT_ERR_ALERT_FATAL_;
                    }
                },
                nullptr);
            return {};
        }

        expected<void> TLSConfig::set_key_log_callback(void (*global_log)(view::rvec line, void* v), void* v) {
            if (!global_log) {
                return unexpect(error::Error("key log callback is nullptr", error::Category::lib, error::fnet_tls_usage_error));
            }
            CHECK_CTX(c)
            auto holder = static_cast<SSLContextHolder*>(lazy::ssl::SSL_CTX_get_ex_data_(c, ssl_import::ssl_appdata_index));
            holder->keylog_callback = global_log;
            holder->keylog_callback_arg = v;
            lazy::ssl::SSL_CTX_set_keylog_callback_(c, [](const ssl_import::SSL* ssl, const char* line) {
                auto ctx = lazy::ssl::SSL_get_SSL_CTX_(ssl);
                auto holder = static_cast<SSLContextHolder*>(lazy::ssl::SSL_CTX_get_ex_data_(ctx, ssl_import::ssl_appdata_index));
                assert(ctx == holder->ctx);
                if (!holder->keylog_callback) {
                    return;
                }
                holder->keylog_callback(line, holder->keylog_callback_arg);
            });
            return {};
        }

        expected<void> TLSConfig::set_session_callback(bool (*cb)(Session&& sess, void* arg), void* arg) {
            CHECK_CTX(c)
            auto holder = static_cast<SSLContextHolder*>(lazy::ssl::SSL_CTX_get_ex_data_(c, ssl_import::ssl_appdata_index));
            holder->session_callback = cb;
            holder->sess_callback_arg = arg;
            lazy::ssl::SSL_CTX_set_session_cache_mode_(c, ssl_import::SSL_SESS_CACHE_CLIENT_ | ssl_import::SSL_SESS_CACHE_SERVER_);
            lazy::ssl::SSL_CTX_sess_set_new_cb_(c, [](ssl_import::SSL* ssl, ssl_import::SSL_SESSION* sess) {
                auto ctx = lazy::ssl::SSL_get_SSL_CTX_(ssl);
                auto holder = static_cast<SSLContextHolder*>(lazy::ssl::SSL_CTX_get_ex_data_(ctx, ssl_import::ssl_appdata_index));
                assert(ctx == holder->ctx);
                Session s{sess};
                return holder->session_callback(sess, holder->sess_callback_arg) ? 1 : 0;
            });
            return {};
        }

        TLSConfig::~TLSConfig() {
            if (ctx) {
                lazy::ssl::SSL_CTX_free_(static_cast<ssl_import::SSL_CTX*>(ctx));
                ctx = nullptr;
            }
        }
    }  // namespace fnet::tls
}  // namespace futils
