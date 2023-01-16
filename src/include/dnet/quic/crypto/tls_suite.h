/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// tls_cipher - tls cipher type
#pragma once
#include <cstddef>
#include "../../../view/iovec.h"

namespace utils {
    namespace dnet::quic::crypto::tls {
        // TLSSuite which is supprted by QUIC
        enum class TLSSuite {
            AES_128_GCM_SHA256,
            AES_256_GCM_SHA384,
            CHACHA20_POLY1305_SHA256,
            AES_128_CCM_SHA256,

            Unsupported,
        };

        constexpr size_t hash_len(TLSSuite suite) noexcept {
            if (suite == TLSSuite::AES_128_CCM_SHA256 || suite == TLSSuite::AES_128_GCM_SHA256 ||
                suite == TLSSuite::CHACHA20_POLY1305_SHA256) {
                return 16;
            }
            else if (suite == TLSSuite::AES_256_GCM_SHA384) {
                return 32;
            }
            return 0;
        }

        inline TLSSuite to_suite(const char* rfcname) noexcept {
            auto cmp = view::rvec(rfcname);
            if (cmp == "TLS_AES_128_GCM_SHA256") {
                return TLSSuite::AES_128_GCM_SHA256;
            }
            else if (cmp == "TLS_AES_256_GCM_SHA384") {
                return TLSSuite::AES_256_GCM_SHA384;
            }
            else if (cmp == "TLS_CHACHA20_POLY1305_SHA256") {
                return TLSSuite::CHACHA20_POLY1305_SHA256;
            }
            else if (cmp == "TLS_AES_128_CCM_SHA256") {
                return TLSSuite::AES_128_CCM_SHA256;
            }
            return TLSSuite::Unsupported;
        }

        constexpr bool is_AES_based(TLSSuite suite) {
            return suite == TLSSuite::AES_128_CCM_SHA256 ||
                   suite == TLSSuite::AES_128_GCM_SHA256 ||
                   suite == TLSSuite::AES_256_GCM_SHA384;
        }

        constexpr bool is_CHACHA20_based(TLSSuite suite) {
            return suite == TLSSuite::CHACHA20_POLY1305_SHA256;
        }
    }  // namespace dnet::quic::crypto::tls
}  // namespace utils
