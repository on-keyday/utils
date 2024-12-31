/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
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
#include "../../strutil/equal.h"
#include "../error.h"
#include "../dll/dll_path.h"
#include "config.h"
#include "session.h"

namespace futils {
    namespace fnet {

        namespace tls {

            fnet_dll_export(bool) isTLSBlock(const error::Error& err);

            // TLS is wrapper class of SSL
            struct fnet_class_export TLS {
               private:
                void* opt;

                // for quic callback invocation
                friend int quic_callback(void* ctx, const quic::crypto::MethodArgs& args);

                constexpr TLS(void* o)
                    : opt(o) {}

                friend fnet_dll_export(expected<TLS>) create_tls_with_error(const TLSConfig&);
                friend fnet_dll_export(expected<TLS>) create_quic_tls_with_error(const TLSConfig&, int (*cb)(void*, quic::crypto::MethodArgs), void* user);

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

                expected<void> set_verify(VerifyMode mode, int (*verify_callback)(int, void*) = nullptr);
                expected<void> set_client_cert_file(const char* cert);
                expected<void> set_cert_chain(const char* pubkey, const char* prvkey);
                expected<void> set_alpn(view::rvec alpn);
                expected<void> set_hostname(const char* hostname, bool verify = true);
                expected<void> set_early_data_enabled(bool enable);

                expected<void> get_early_data_accepted();

                // quic extensions
                expected<void> provide_quic_data(quic::crypto::EncryptionLevel level, view::rvec data);
                expected<void> progress_quic();
                expected<void> set_quic_transport_params(view::rvec data);
                expected<view::rvec> get_peer_quic_transport_params();
                expected<void> set_quic_early_data_context(view::rvec data);

                // return (remain,err)
                expected<view::rvec> provide_tls_data(view::rvec data);
                // return (read,err)
                expected<view::wvec> receive_tls_data(view::wvec data);

                // this returns error except block,
                // so we don't have to check error with isTLSBlock
                expected<size_t> receive_tls_data_until_block(auto&& callback, view::wvec buffer = {}) {
                    auto do_receive = [&]() -> expected<size_t> {
                        size_t size = 0;
                        while (true) {
                            auto res = receive_tls_data(buffer);
                            if (!res) {
                                if (isTLSBlock(res.error())) {
                                    return size;
                                }
                                return res.transform([&](view::wvec) { return size; });
                            }
                            callback(*res);
                        }
                    };
                    if (buffer.empty()) {
                        byte buf[1024];
                        buffer = view::wvec(buf, 1024);
                        return do_receive();
                    }
                    return do_receive();
                }

                // TLS connection methods
                // call setup_ssl before call these methods

                expected<view::rvec> write(view::rvec data);
                expected<view::wvec> read(view::wvec data);

                // this returns error except block,
                // so we don't have to check error with isTLSBlock
                expected<size_t> read_until_block(auto&& callback, view::wvec buffer = {}) {
                    auto do_read = [&]() -> expected<size_t> {
                        size_t size = 0;
                        while (true) {
                            auto res = read(buffer);
                            if (!res) {
                                if (isTLSBlock(res.error())) {
                                    return size;
                                }
                                return res.transform([&](view::wvec) { return size; });
                            }
                            size += res->size();
                            callback(*res);
                        }
                    };
                    if (buffer.empty()) {
                        byte buf[1024];
                        buffer = view::wvec(buf, 1024);
                        return do_read();
                    }
                    return do_read();
                }

                expected<void> shutdown();

                // TLS Handshake methods
                // this can be called both by TLS and by QUIC

                expected<void> connect();
                expected<void> accept();

                expected<void> verify_ok();

                bool has_ssl() const;

                expected<view::rvec> get_selected_alpn();

                expected<TLSCipher> get_cipher();

                expected<void> set_session(const Session& sess);
                expected<Session> get_session();

                bool in_handshake();

                expected<view::rvec> get_tls_version();
            };

            void get_error_strings(int (*cb)(const char*, size_t, void*), void* user);

            template <class Fn>
            void get_error_strings(Fn&& fn) {
                auto ptr = std::addressof(fn);
                get_errors(
                    [](const char* msg, size_t len, void* p) -> int {
                        return (*decltype(ptr)(p))(msg, len);
                    },
                    ptr);
            }

            fnet_dll_export(void) set_libssl(lazy::dll_path path);
            fnet_dll_export(void) set_libcrypto(lazy::dll_path path);

            // new TLS create
            fnet_dll_export(expected<TLS>) create_tls_with_error(const TLSConfig&);
            fnet_dll_export(expected<TLS>) create_quic_tls_with_error(const TLSConfig&, int (*cb)(void*, quic::crypto::MethodArgs), void* user);
            inline TLS create_tls(const TLSConfig& conf) {
                return create_tls_with_error(conf).value_or(TLS{});
            }

            template <class T>
            expected<TLS> create_quic_tls_with_error(const TLSConfig& conf, int (*cb)(T*, quic::crypto::MethodArgs), std::type_identity_t<T>* user) {
                return create_quic_tls_with_error(conf, reinterpret_cast<int (*)(void*, quic::crypto::MethodArgs)>(cb), static_cast<void*>(user));
            }

            template <class T>
            inline TLS create_quic_tls(const TLSConfig& conf, int (*cb)(T*, quic::crypto::MethodArgs), std::type_identity_t<T>* user) {
                return create_quic_tls_with_error(conf, cb, user).value_or(TLS{});
            }

            // load libraries
            fnet_dll_export(bool) load_crypto();
            fnet_dll_export(bool) load_ssl();

            // judge loaded library
            // WARNING(on-keyday): this function judge library
            // using only one function for each library specific
            fnet_dll_export(bool) is_open_ssl();
            fnet_dll_export(bool) is_boring_ssl();

            fnet_dll_export(view::rvec) get_alert_desc(futils::byte alert_code);

        }  // namespace tls
    }  // namespace fnet
}  // namespace futils
