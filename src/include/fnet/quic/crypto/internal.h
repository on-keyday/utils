/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// internal - internal functions
#pragma once
#include "../../../core/byte.h"
#include "tls_suite.h"

namespace futils {
    namespace fnet {
        namespace tls {
            struct TLSCipher;
        }
        namespace quic::crypto {
            // internal functions
            bool generate_masks_aes_based(const byte* hp_key, const byte* sample, byte* masks, bool is_aes256);
            bool generate_masks_chacha20_based(const byte* hp_key, const byte* sample, byte* masks);
            // returns (is_AES,is_CHACHA,hash_len,ok)
            tls::QUICCipherSuite judge_cipher(const tls::TLSCipher& cipher);
        }  // namespace quic::crypto
    }      // namespace fnet
}  // namespace futils
