/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../dll/dllh.h"
#include "../error.h"
#include "../quic/crypto/enc_level.h"
#include "../../helper/equal.h"
#include "alpn.h"

namespace utils {
    namespace fnet {
        namespace tls {
            // TLSCipher is tls cipher information wrapper
            // wrapper of SSL_CIPHER object
            struct fnet_class_export TLSCipher {
               private:
                const void* cipher = nullptr;
                friend TLSCipher make_cipher(const void*);
                constexpr TLSCipher(const void* c)
                    : cipher(c) {}

               public:
                constexpr TLSCipher() = default;
                constexpr TLSCipher(const TLSCipher&) = default;
                const char* name() const;
                const char* rfcname() const;
                int bits(int* bit = nullptr) const;
                int nid() const;

                constexpr explicit operator bool() const {
                    return cipher != nullptr;
                }

                bool is_algorithm(const char* alg_rfc_name) const {
                    return helper::equal(rfcname(), alg_rfc_name);
                }

                constexpr bool operator==(const TLSCipher& c) const {
                    return cipher == c.cipher;
                }
            };
        }  // namespace tls

        namespace quic::crypto {
            // for QUIC-TLS callback wrapper arguments
            struct MethodArgs {
                ArgType type;
                EncryptionLevel level;
                tls::TLSCipher cipher;
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

        }  // namespace quic::crypto

        namespace tls {
            struct TLS;

            struct ALPNCallback {
                bool (*select)(ALPNSelector& selector, void*) = nullptr;
                void* arg = nullptr;
            };

            // TLSConfig is wrapper class of SSL_CTX
            struct fnet_class_export TLSConfig {
               private:
                void* ctx = nullptr;
                friend fnet_dll_export(TLSConfig) configure();
                friend fnet_dll_export(std::pair<TLS, error::Error>) create_tls_with_error(const TLSConfig&);
                friend fnet_dll_export(std::pair<TLS, error::Error>) create_quic_tls_with_error(const TLSConfig&, int (*cb)(void*, quic::crypto::MethodArgs), void* user);

               public:
                constexpr TLSConfig() = default;
                constexpr TLSConfig(TLSConfig&& conf)
                    : ctx(std::exchange(conf.ctx, nullptr)) {}
                ~TLSConfig();
                TLSConfig& operator=(TLSConfig&& conf) {
                    if (this == &conf) {
                        return *this;
                    }
                    this->~TLSConfig();
                    ctx = std::exchange(conf.ctx, nullptr);
                    return *this;
                }

                error::Error set_cacert_file(const char* cacert, const char* dir = nullptr);
                error::Error set_verify(int mode, int (*verify_callback)(int, void*) = nullptr);
                error::Error set_client_cert_file(const char* cert);
                error::Error set_alpn(view::rvec alpn);
                error::Error set_cert_chain(const char* pubkey, const char* prvkey);
                error::Error set_eraly_data_enabled(bool enable);
                // if selector select protocol name and returns true,
                // use it as negotiated protocol
                // otherwise if returns true then alert warning
                // if callback returns false then alert fatal error
                // manage ALPNCallback yourself while connections alive
                error::Error set_alpn_select_callback(const ALPNCallback* cb);
            };

            // configure() returns TLSConfig object initialized with SSL_CTX object
            fnet_dll_export(TLSConfig) configure();
        }  // namespace tls
    }      // namespace fnet
}  // namespace utils
