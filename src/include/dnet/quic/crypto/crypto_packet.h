/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto_packet - crypto packet info
#pragma once
#include "../../bytelen.h"

namespace utils {
    namespace dnet {
        namespace quic::crypto {
            // On QUIC version 1, this is 16 byte
            // see also:
            // https://tex2e.github.io/rfc-translater/html/rfc9001.html#5-3--AEAD-Usage
            // https://tex2e.github.io/rfc-translater/html/rfc9001.html#A-2--Client-Initial
            constexpr auto authentication_tag_length = 16;

            struct CryptoPacketInfo {
                // head is header of QUIC packet without packet number
                ByteLen head;
                // payload is payload of QUIC packet including packet number
                // payload.data must be adjacent to head.data
                ByteLen payload;
                // tag is payload end tag area of QUIC packet
                // for QUIC v1, tag length is 16
                // tag.data must be adjacent to payload.data
                ByteLen tag;
                // processed_payload is encrypted/decrypted payload without packet number
                // when encoding include tag space (QUICv1:16byte)
                // when decoding exclude tag space
                ByteLen processed_payload;

                ByteLen composition() const {
                    if (!head.adjacent(payload) || !payload.adjacent(tag)) {
                        return {};
                    }
                    return ByteLen{head.data, head.len + payload.len + tag.len};
                }
            };
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
