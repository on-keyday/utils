/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/dll/lazy/ssldll.h>
#include <fnet/tls/tls.h>
#include <fnet/tls/liberr.h>

namespace utils {
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
            err.code = get_error_code();
            if (cant_load) {
                err.alt_err = error::Error("cannot decide OpenSSL/BoringSSL. maybe diffrent type library?", error::ErrorCategory::cryptoerr);
                return err;
            }
            if (err.code == 0) {
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
            helper::appends(pb, name, "=error:");
            number::to_string(pb, code, 16);
            helper::append(pb, ":");
            if (is_boring_ssl()) {
                auto str = lazy::crypto::bssl::ERR_lib_error_string_(code);
                if (str) {
                    helper::append(pb, str);
                }
                helper::append(pb, "::");
                str = lazy::crypto::bssl::ERR_reason_error_string_(code);
                if (str) {
                    helper::append(pb, str);
                }
            }
            else if (is_open_ssl()) {
                auto str = lazy::crypto::ossl::ERR_lib_error_string_(code);
                if (str) {
                    helper::append(pb, str);
                }
                helper::append(pb, "::");
                str = lazy::crypto::ossl::ERR_reason_error_string_(code);
                if (str) {
                    helper::append(pb, str);
                }
            }
            else {
                helper::append(pb, "::");
            }
        }

        void LibError::error(helper::IPushBacker<> pb) const {
            if (ssl_code != 0) {
                helper::append(pb, "ssl_error_code=");
                number::to_string(pb, ssl_code);
                if (lazy::ssl::bssl::sp::SSL_error_description_.find()) {  // on boringssl
                    helper::append(pb, " ");
                    auto str = lazy::ssl::bssl::sp::SSL_error_description_(ssl_code);
                    if (str) {
                        helper::appends(pb, "ssl_error_desc=\"", str, "\" ");
                    }
                }
            }
            if (code != 0) {
                if (ssl_code != 0) {
                    helper::append(pb, " ");
                }
                print_lib_error(pb, "lib_error", code);
            }
            if (alt_err) {
                helper::append(pb, " ");
                alt_err.error(pb);
            }
        }

        void LibSubError::error(helper::IPushBacker<> pb) const {
            print_lib_error(pb, "sub_error", code);
            if (alt_err) {
                helper::append(pb, " ");
                alt_err.error(pb);
            }
        }

        struct SSLContextHolder {
            ssl_import::SSL_CTX* ctx = nullptr;
            void (*alpn_callback)(ALPNSelector& sel, void*) = nullptr;
            void* alpn_callback_arg = nullptr;
        };

        fnet_dll_implement(TLSConfig) configure() {
            TLSConfig conf;
            if (!load_ssl()) {
                return {};
            }
            conf.ctx = lazy::ssl::SSL_CTX_new_(lazy::ssl::TLS_method_());
            return conf;
        }
        constexpr auto errNotInitialized = error::Error("TLSConfig is not initialized. call configure() to initialize.", error::ErrorCategory::sslerr);
        constexpr error::Error errLibJudge = error::Error("library type judgement failure. maybe other type library?", error::ErrorCategory::fneterr);

#define CHECK_CTX(c)              \
    if (!ctx) {                   \
        return errNotInitialized; \
    }                             \
    ssl_import::SSL_CTX* c = static_cast<ssl_import::SSL_CTX*>(ctx);

        error::Error TLSConfig::set_cacert_file(const char* cacert, const char* dir) {
            CHECK_CTX(c)
            if (!lazy::ssl::SSL_CTX_load_verify_locations_(c, cacert, dir)) {
                return libError("SSL_CTX_load_verify_locations", "failed to load CA file or CA file directory location");
            }
            return error::none;
        }

        error::Error TLSConfig::set_verify(int mode, int (*verify_callback)(int, void*)) {
            CHECK_CTX(c)
            lazy::ssl::SSL_CTX_set_verify_(c, mode,
                                           (int (*)(int, ssl_import::X509_STORE_CTX*))(verify_callback));
            return error::none;
        }

        error::Error TLSConfig::set_client_cert_file(const char* cert) {
            CHECK_CTX(c)
            auto ptr = lazy::ssl::SSL_load_client_CA_file_(cert);
            if (!ptr) {
                return libError("SSL_load_client_CA_file", "failed to load client CA file");
            }
            lazy::ssl::SSL_CTX_set_client_CA_list_(c, ptr);
            return error::none;
        }

        error::Error TLSConfig::set_cert_chain(const char* pubkey, const char* prvkey) {
            CHECK_CTX(c)
            auto res = lazy::ssl::SSL_CTX_use_certificate_chain_file_(c, pubkey);
            if (!res) {
                return libError("SSL_CTX_use_certificate_chain_file", "failed to load public key (certificate)");
            }
            res = lazy::ssl::SSL_CTX_use_PrivateKey_file_(c, prvkey, ssl_import::SSL_FILETYPE_PEM_);
            if (!res) {
                return libError("SSL_CTX_use_PrivateKey_file", "failed to load private key");
            }
            if (!lazy::ssl::SSL_CTX_check_private_key_(c)) {
                return libError("SSL_CTX_check_private_key", "private key and public key are not matched or invalid");
            }
            return error::none;
        }

        error::Error TLSConfig::set_alpn(view::rvec alpn) {
            CHECK_CTX(c)
            int res = 0;
            if (is_boring_ssl()) {
                res = lazy::ssl::bssl::SSL_CTX_set_alpn_protos_(c, alpn.data(), alpn.size());
            }
            else if (is_open_ssl()) {
                res = lazy::ssl::ossl::SSL_CTX_set_alpn_protos_(c, alpn.data(), alpn.size());
            }
            else {
                return errLibJudge;
            }
            if (res != 0) {
                return libError("SSL_CTX_set_alpn_protos", "failed to set ALPN", nullptr, res);
            }
            return error::none;
        }

        error::Error TLSConfig::set_eraly_data_enabled(bool enable) {
            CHECK_CTX(c);
            lazy::ssl::SSL_CTX_set_early_data_enabled_(c, enable ? 1 : 0);
            return error::none;
        }

        error::Error TLSConfig::set_alpn_select_callback(const ALPNCallback* cb) {
            if (!cb) {
                return error::Error("invalid argument");
            }
            CHECK_CTX(c)
            lazy::ssl::SSL_CTX_set_alpn_select_cb_(
                c, [](ssl_import::SSL* ssl, const uint8_t** out, uint8_t* out_len, const uint8_t* in, unsigned int in_len, void* arg) {
                    ALPNSelector sel{in, in_len, out, out_len};
                    auto cb = static_cast<const ALPNCallback*>(arg);
                    if (!cb) {
                        return ssl_import::SSL_TLSEXT_ERR_NOACK_;
                    }
                    if (cb->select(sel, cb->arg)) {
                        if (*out) {
                            return ssl_import::SSL_TLSEXT_ERR_OK_;
                        }
                        return ssl_import::SSL_TLSEXT_ERR_NOACK_;
                    }
                    else {
                        return ssl_import::SSL_TLSEXT_ERR_ALERT_FATAL_;
                    }
                },
                const_cast<ALPNCallback*>(cb));
            return error::none;
        }

        TLSConfig::~TLSConfig() {
            if (ctx) {
                lazy::ssl::SSL_CTX_free_(static_cast<ssl_import::SSL_CTX*>(ctx));
                ctx = nullptr;
            }
        }
    }  // namespace fnet::tls
}  // namespace utils
