/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace utils {
    namespace dnet::quic::crypto {
        // On QUIC version 1, this is 16 byte
        // see also:
        // https://tex2e.github.io/rfc-translater/html/rfc9001.html#5-3--AEAD-Usage
        // https://tex2e.github.io/rfc-translater/html/rfc9001.html#A-2--Client-Initial
        constexpr auto authentication_tag_length = 16;

        constexpr auto sample_skip_size = 4;

    }  // namespace dnet::quic::crypto
}  // namespace utils
