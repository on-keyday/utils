/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// encdec_suite - encryption/decryption suite
#pragma once
#include "keys.h"
#include "../../tls/tls.h"

namespace futils {
    namespace fnet::quic::crypto {
        enum class KeyPhase {
            none,
            one,
            zero,
        };

        struct EncDecSuite {
            const HP* hp = nullptr;
            const KeyIV* keyiv = nullptr;
            const tls::TLSCipher* cipher = nullptr;
            KeyPhase phase = KeyPhase::none;
        };

    }  // namespace fnet::quic::crypto
}  // namespace futils
