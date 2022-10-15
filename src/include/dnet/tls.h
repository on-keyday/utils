/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// tls - SSL/TLS
#pragma once
#include "dll/dllh.h"
#include <utility>
#include <cstddef>
#include <memory>
#include "httpstring.h"
#include "quic/crypto.h"

namespace utils {
    namespace dnet {
        enum class TLSIOState {
            idle,
            from_ssl,
            to_ssl,
            fatal,
            to_provider,
            from_provider,
        };

        struct TLSIO {
           private:
            TLSIOState state = TLSIOState::idle;
            friend struct TLS;

           public:
            bool as_server() {
                if (state == TLSIOState::idle) {
                    state = TLSIOState::to_ssl;
                    return true;
                }
                return false;
            }

            bool as_client() {
                if (state == TLSIOState::idle) {
                    state = TLSIOState::from_ssl;
                    return true;
                }
                return false;
            }

            constexpr bool failed() const {
                return state == TLSIOState::fatal;
            }
        };

        namespace quic {

            enum class ArgType {
                // openssl
                // read_secret,write_secret,secret_len,level is enabled
                secret,
                // boring ssl
                // cipher,write_secret,secret_len,level is enabled
                wsecret,
                // boring ssl
                // cipher,read_secret,secret_len,level is enabled
                rsecret,
                // data,len,level is enabled
                handshake_data,
                // nonthing enabled
                flush,
                // alert is enabled
                alert,
            };

            enum class EncryptionLevel {
                initial = 0,
                early_data,
                handshake,
                application,
            };

            struct MethodArgs {
                ArgType type;
                EncryptionLevel level;
                const void* cipher;
                union {
                    const byte* read_secret;
                    const byte* data;
                };
                const byte* write_secret;
                union {
                    size_t len;
                    size_t secret_len;
                    byte alert;
                };
            };
        }  // namespace quic

        // TLS is wrapper class of OpenSSL/BoringSSL library
        struct dnet_class_export TLS {
           private:
            int err;
            size_t prevred;
            void* opt;

            // for quic callback invocation
            friend int quic_callback(TLS& tls, const quic::MethodArgs& args);

            constexpr TLS(void* o)
                : opt(o), err(0), prevred(0) {}
            friend dnet_dll_export(TLS) create_tls();
            friend dnet_dll_export(TLS) create_tls_from(const TLS& tls);
            constexpr TLS(int err)
                : opt(nullptr), err(err), prevred(0) {}

           public:
            constexpr TLS()
                : TLS(nullptr) {}
            constexpr TLS(TLS&& tls)
                : opt(std::exchange(tls.opt, nullptr)),
                  err(std::exchange(tls.err, 0)) {}
            TLS& operator=(TLS&& tls) {
                if (this == &tls) {
                    return *this;
                }
                this->~TLS();
                opt = std::exchange(tls.opt, nullptr);
                err = std::exchange(tls.err, 0);
                return *this;
            }
            ~TLS();
            constexpr operator bool() const {
                return opt != nullptr;
            }
            // make_ssl set up SSL structure for TLS
            bool make_ssl();

            bool set_cacert_file(const char* cacert, const char* dir = nullptr);
            bool set_verify(int mode, int (*verify_callback)(int, void*) = nullptr);
            bool set_alpn(const void* p, size_t len);
            bool set_hostname(const char* hostname);
            bool set_client_cert_file(const char* cert);
            bool set_cert_chain(const char* pubkey, const char* prvkey);

            // make_quic set up SSL structure for QUIC
            bool make_quic(int (*cb)(void*, quic::MethodArgs), void* user);

            template <class T>
            bool make_quic(int (*cb)(T*, quic::MethodArgs), std::type_identity_t<T>* user) {
                auto cb_void = reinterpret_cast<int (*)(void*, quic::MethodArgs)>(cb);
                void* user_void = user;
                return make_quic(cb_void, user_void);
            }

            bool set_quic_transport_params(const void* params, size_t len);

            constexpr size_t readsize() const {
                return prevred;
            }

            bool provide_tls_data(const void* data, size_t len, size_t* written = nullptr);
            bool receive_tls_data(void* data, size_t len);

            bool provide_quic_data(quic::EncryptionLevel level, const void* data, size_t len);
            bool progress_quic();

            bool write(const void* data, size_t len, size_t* written = nullptr);
            bool read(void* data, size_t len);

            bool connect();
            bool accept();

            bool shutdown();

            bool block() const;

            bool closed() const;

            constexpr int geterr() const {
                return err;
            }

            bool verify_ok();

            bool has_ssl() const;
            bool has_sslctx() const;

            bool get_alpn(const char** selected, unsigned int* len);

            const char* get_alpn(unsigned int* len = nullptr) {
                unsigned int l = 0;
                if (!len) {
                    len = &l;
                }
                const char* sel = nullptr;
                get_alpn(&sel, len);
                return sel;
            }

            static void get_errors(int (*cb)(const char*, size_t, void*), void* user);

            template <class Fn>
            static void get_errors(Fn&& fn) {
                auto ptr = std::addressof(fn);
                get_errors(
                    [](const char* msg, size_t len, void* p) -> int {
                        return (*decltype(ptr)(p))(msg, len);
                    },
                    ptr);
            }

            constexpr void clear_err() {
                err = 0;
            }

            constexpr void clear_readsize() {
                prevred = 0;
            }
        };

        dnet_dll_export(void) set_libssl(const char* path);
        dnet_dll_export(void) set_libcrypto(const char* path);
        dnet_dll_export(TLS) create_tls();
        dnet_dll_export(TLS) create_tls_from(const TLS& tls);

        // load libraries
        dnet_dll_export(bool) load_crypto();
        dnet_dll_export(bool) load_ssl();
        dnet_dll_export(bool) is_boringssl_ssl();
        dnet_dll_export(bool) is_openssl_ssl();
        dnet_dll_export(bool) is_boringssl_crypto();
        dnet_dll_export(bool) is_openssl_crypto();

        dnet_dll_export(size_t) tls_input(TLS& tls, String& buf, size_t* next_expect);
        dnet_dll_export(size_t) tls_output(TLS& tls, String& buf, String& tmpbuf);
    }  // namespace dnet
}  // namespace utils
