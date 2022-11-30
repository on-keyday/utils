/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../bytelen.h"
#include <cstddef>

namespace utils {
    namespace dnet {
        namespace quic::crypto {
            template <size_t size_>
            struct Key {
                byte key[size_];

                constexpr size_t size() const {
                    return size_;
                }
            };

            struct Keys {
                byte resource[32 + 12 + 32 + 32];
                ByteLen key;
                ByteLen iv;
                ByteLen hp;
                ByteLen ku;
            };

            constexpr Key<8> quic_key{'q', 'u', 'i', 'c', ' ', 'k', 'e', 'y'};
            constexpr Key<7> quic_iv{'q', 'u', 'i', 'c', ' ', 'i', 'v'};
            constexpr Key<7> quic_hp{'q', 'u', 'i', 'c', ' ', 'h', 'p'};
            constexpr Key<9> server_in{'s', 'e', 'r', 'v', 'e', 'r', ' ', 'i', 'n'};
            constexpr Key<9> client_in{'c', 'l', 'i', 'e', 'n', 't', ' ', 'i', 'n'};
            constexpr Key<7> quic_ku{'q', 'u', 'i', 'c', ' ', 'k', 'u'};

            constexpr Key<20> quic_v1_initial_salt = {0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34,
                                                      0xb3, 0x4d, 0x17, 0x9a, 0xe6, 0xa4, 0xc8,
                                                      0x0c, 0xad, 0xcc, 0xbb, 0x7f, 0x0a};

            constexpr Key<32> quic_v1_retry_integrity_tag_salt = {0xd9, 0xc9, 0x94, 0x3e, 0x61, 0x01, 0xfd, 0x20,
                                                                  0x00, 0x21, 0x50, 0x6b, 0xcc, 0x02, 0x81, 0x4c,
                                                                  0x73, 0x03, 0x0f, 0x25, 0xc7, 0x9d, 0x71, 0xce,
                                                                  0x87, 0x6e, 0xca, 0x87, 0x6e, 0x6f, 0xca, 0x8e};

            constexpr Key<16> quic_v1_retry_integrity_tag_key = {0xbe, 0x0c, 0x69, 0x0b, 0x9f, 0x66, 0x57, 0x5a,
                                                                 0x1d, 0x76, 0x6b, 0x54, 0xe3, 0x68, 0xc8, 0x4e};

            constexpr Key<12> quic_v1_retry_integrity_tag_nonce = {0x46, 0x15, 0x99, 0xd3, 0x5d, 0x63, 0x2b, 0xf2,
                                                                   0x23, 0x98, 0x25, 0xbb};

            enum class ArgType {
                // openssl
                // read_secret,write_secret,secret_len,level is enabled
                secret,
                // boring ssl
                // cipher,write_secret,secret_len,level is enabled
                wsecret,
                // boring ssl
                // cipher,read_secret,secret_len,level is enabled
                rsecret,
                // data,len,level is enabled
                handshake_data,
                // nonthing enabled
                flush,
                // alert is enabled
                alert,
            };

            enum class EncryptionLevel {
                initial = 0,
                early_data,
                handshake,
                application,
            };
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
