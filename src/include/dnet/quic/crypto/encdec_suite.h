/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// encdec_suite - encryption/decryption suite
#pragma once
#include "keys.h"
#include "../../tls.h"

namespace utils {
    namespace dnet::quic::crypto {
        struct EncryptSuite {
            const Keys* keys;
            const TLSCipher* cipher;
        };

        struct DecryptSuite {
            const Keys* keys;
            const TLSCipher* cipher;
            std::uint64_t largest_pn;
        };

    }  // namespace dnet::quic::crypto
}  // namespace utils
