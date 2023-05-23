/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// tls - wrapper classes of OpenSSL/BoringSSL
// because this library mentioning about OpenSSL and to avoid lawsuit
// these two note is written
// * This product includes cryptographic software written by Eric Young (eay@cryptsoft.com)
// * This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)
// but unfortunately, this library uses no source code or binary written by OpenSSL except only forward declarations
// forward declarations are copied by myself, sorted,and changed some details
#pragma once
#include "../dll/dllh.h"
#include <utility>
#include <cstddef>
#include <memory>
#include "../../helper/strutil.h"
#include "../../helper/equal.h"
#include "../error.h"
#include "../dll/dll_path.h"
#include "config.h"

namespace utils {
    namespace fnet {

        namespace tls {

            // OPENSSL verify mode. used by TLS.set_verify
            constexpr auto SSL_VERIFY_NONE_ = 0x00;
            constexpr auto SSL_VERIFY_PEER_ = 0x01;
            constexpr auto SSL_VERIFY_FAIL_IF_NO_PEER_CERT_ = 0x02;
            constexpr auto SSL_VERIFY_CLIENT_ONCE_ = 0x04;
            constexpr auto SSL_VERIFY_POST_HANDSHAKE_ = 0x08;

            fnet_dll_export(bool) isTLSBlock(const error::Error& err);

            // TLS is wrapper class of SSL
            struct fnet_class_export TLS {
               private:
                void* opt;

                // for quic callback invocation
                friend int quic_callback(void* ctx, const quic::crypto::MethodArgs& args);

                constexpr TLS(void* o)
                    : opt(o) {}

                friend fnet_dll_export(std::pair<TLS, error::Error>) create_tls_with_error(const TLSConfig&);
                friend fnet_dll_export(std::pair<TLS, error::Error>) create_quic_tls_with_error(const TLSConfig&, int (*cb)(void*, quic::crypto::MethodArgs), void* user);

               public:
                constexpr TLS()
                    : TLS(nullptr) {}
                constexpr TLS(TLS&& tls)
                    : opt(std::exchange(tls.opt, nullptr)) {}
                TLS& operator=(TLS&& tls) {
                    if (this == &tls) {
                        return *this;
                    }
                    this->~TLS();
                    opt = std::exchange(tls.opt, nullptr);
                    return *this;
                }
                ~TLS();
                constexpr operator bool() const {
                    return opt != nullptr;
                }

                error::Error set_verify(int mode, int (*verify_callback)(int, void*) = nullptr);
                error::Error set_client_cert_file(const char* cert);
                error::Error set_cert_chain(const char* pubkey, const char* prvkey);
                error::Error set_alpn(view::rvec alpn);
                error::Error set_hostname(const char* hostname, bool verify = true);
                error::Error set_eraly_data_enabled(bool enable);

                bool get_early_data_accepted();

                // quic extensions
                error::Error provide_quic_data(quic::crypto::EncryptionLevel level, view::rvec data);
                error::Error progress_quic();
                error::Error set_quic_transport_params(view::rvec data);
                view::rvec get_peer_quic_transport_params();
                error::Error set_quic_eraly_data_context(view::rvec data);

                // return (remain,err)
                std::pair<view::rvec, error::Error> provide_tls_data(view::rvec data);
                // return (read,err)
                std::pair<view::wvec, error::Error> receive_tls_data(view::wvec data);

                // TLS connection methods
                // call setup_ssl before call these methods

                std::pair<view::rvec, error::Error> write(view::rvec data);
                std::pair<view::wvec, error::Error> read(view::wvec data);

                error::Error shutdown();

                // TLS Handshake methods
                // this can be called both by TLS and by QUIC

                error::Error connect();
                error::Error accept();

                error::Error verify_ok();

                bool has_ssl() const;

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

                TLSCipher get_cipher();
            };

            fnet_dll_export(void) set_libssl(lazy::dll_path path);
            fnet_dll_export(void) set_libcrypto(lazy::dll_path path);

            // new TLS create
            fnet_dll_export(std::pair<TLS, error::Error>) create_tls_with_error(const TLSConfig&);
            fnet_dll_export(std::pair<TLS, error::Error>) create_quic_tls_with_error(const TLSConfig&, int (*cb)(void*, quic::crypto::MethodArgs), void* user);
            inline TLS create_tls(const TLSConfig& conf) {
                auto [tls, _] = create_tls_with_error(conf);
                return std::move(tls);
            }
            fnet_dll_export(std::pair<TLS, error::Error>) create_quic_tls_with_error(const TLSConfig&, int (*cb)(void*, quic::crypto::MethodArgs), void* user);

            template <class T>
            std::pair<TLS, error::Error> create_quic_tls_with_error(const TLSConfig& conf, int (*cb)(T*, quic::crypto::MethodArgs), std::type_identity_t<T>* user) {
                return create_quic_tls_with_error(conf, reinterpret_cast<int (*)(void*, quic::crypto::MethodArgs)>(cb), static_cast<void*>(user));
            }

            template <class T>
            inline TLS create_quic_tls(const TLSConfig& conf, int (*cb)(T*, quic::crypto::MethodArgs), std::type_identity_t<T>* user) {
                auto [tls, _] = create_quic_tls_with_error(conf, cb, user);
                return std::move(tls);
            }

            // load libraries
            fnet_dll_export(bool) load_crypto();
            fnet_dll_export(bool) load_ssl();

            // judge loaded library
            // WARNING(on-keyday): this function judge library
            // using only one function for each library specific
            fnet_dll_export(bool) is_open_ssl();
            fnet_dll_export(bool) is_boring_ssl();

        }  // namespace tls
    }      // namespace fnet
}  // namespace utils
