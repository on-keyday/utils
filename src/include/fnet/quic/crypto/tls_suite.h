/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// tls_cipher - tls cipher type
#pragma once
#include <cstddef>
#include "../../../view/iovec.h"

namespace futils {
    namespace fnet::tls {
        // TLSSuite which is supprted by QUIC
        enum class QUICCipherSuite {
            AES_128_GCM_SHA256,
            AES_256_GCM_SHA384,
            CHACHA20_POLY1305_SHA256,
            AES_128_CCM_SHA256,

            Unsupported,
        };

        constexpr size_t hash_len(QUICCipherSuite suite) noexcept {
            if (suite == QUICCipherSuite::AES_128_CCM_SHA256 || suite == QUICCipherSuite::AES_128_GCM_SHA256 ||
                suite == QUICCipherSuite::CHACHA20_POLY1305_SHA256) {
                return 16;
            }
            else if (suite == QUICCipherSuite::AES_256_GCM_SHA384) {
                return 32;
            }
            return 0;
        }

        inline QUICCipherSuite to_suite(const char* rfcname) noexcept {
            auto cmp = view::rvec(rfcname);
            if (cmp == "TLS_AES_128_GCM_SHA256") {
                return QUICCipherSuite::AES_128_GCM_SHA256;
            }
            else if (cmp == "TLS_AES_256_GCM_SHA384") {
                return QUICCipherSuite::AES_256_GCM_SHA384;
            }
            else if (cmp == "TLS_CHACHA20_POLY1305_SHA256") {
                return QUICCipherSuite::CHACHA20_POLY1305_SHA256;
            }
            else if (cmp == "TLS_AES_128_CCM_SHA256") {
                return QUICCipherSuite::AES_128_CCM_SHA256;
            }
            return QUICCipherSuite::Unsupported;
        }

        constexpr bool is_AES_based(QUICCipherSuite suite) {
            return suite == QUICCipherSuite::AES_128_CCM_SHA256 ||
                   suite == QUICCipherSuite::AES_128_GCM_SHA256 ||
                   suite == QUICCipherSuite::AES_256_GCM_SHA384;
        }

        constexpr bool is_CHACHA20_based(QUICCipherSuite suite) {
            return suite == QUICCipherSuite::CHACHA20_POLY1305_SHA256;
        }
    }  // namespace fnet::tls
}  // namespace futils
