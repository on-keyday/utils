/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../dll/dllh.h"
#include "../error.h"
#include "../quic/crypto/enc_level.h"
#include "../../strutil/equal.h"
#include "alpn.h"
#include "session.h"
#include <wrap/light/enum.h>

namespace futils {
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
                    return strutil::equal(rfcname(), alg_rfc_name);
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

            enum class VerifyMode {
                none = 0x0,
                peer = 0x1,
                fail_if_no_peer_cert = 0x2,
                client_once = 0x4,
                post_handshake = 0x8,
            };

            DEFINE_ENUM_FLAGOP(VerifyMode)

            /*
            struct ALPNCallback {
                bool (*select)(ALPNSelector& selector, void*) = nullptr;
                void* arg = nullptr;
            };
            */

            // TLSConfig is wrapper class of SSL_CTX
            struct fnet_class_export TLSConfig {
               private:
                void* ctx = nullptr;
                friend fnet_dll_export(expected<TLSConfig>) configure_with_error();
                friend fnet_dll_export(expected<TLS>) create_tls_with_error(const TLSConfig&);
                friend fnet_dll_export(expected<TLS>) create_quic_tls_with_error(const TLSConfig&, int (*cb)(void*, quic::crypto::MethodArgs), void* user);

               public:
                constexpr TLSConfig() = default;
                constexpr TLSConfig(TLSConfig&& conf)
                    : ctx(std::exchange(conf.ctx, nullptr)) {}
                TLSConfig(const TLSConfig& conf);
                ~TLSConfig();
                TLSConfig& operator=(TLSConfig&& conf) {
                    if (this == &conf) {
                        return *this;
                    }
                    this->~TLSConfig();
                    ctx = std::exchange(conf.ctx, nullptr);
                    return *this;
                }

                TLSConfig& operator=(const TLSConfig& conf);

                expected<void> set_cacert_file(const char* cacert, const char* dir = nullptr);
                expected<void> set_verify(VerifyMode mode, int (*verify_callback)(int, void*) = nullptr);
                expected<void> set_client_cert_file(const char* cert);
                expected<void> set_alpn(view::rvec alpn);
                expected<void> set_cert_chain(const char* pubkey, const char* prvkey);
                expected<void> set_early_data_enabled(bool enable);
                // if selector select protocol name and returns true,
                // use it as negotiated protocol
                // otherwise if returns true then alert warning
                // if callback returns false then alert fatal error
                // manage ALPNCallback yourself while connections alive
                expected<void> set_alpn_select_callback(bool (*select)(ALPNSelector& selector, void* arg), void* arg);

                expected<void> set_key_log_callback(void (*global_log)(view::rvec line, void* arg), void* arg);

                expected<void> set_session_callback(bool (*cb)(Session&& sess, void* arg), void* arg);

                constexpr operator bool() const {
                    return ctx != nullptr;
                }
            };

            // configure() returns TLSConfig object initialized with SSL_CTX object
            fnet_dll_export(expected<TLSConfig>) configure_with_error();

            inline TLSConfig configure() {
                return configure_with_error().value_or(TLSConfig{});
            }
        }  // namespace tls
    }  // namespace fnet
}  // namespace futils
