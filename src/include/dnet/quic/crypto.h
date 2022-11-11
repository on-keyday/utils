/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../dll/dllh.h"
#include "crypto_keys.h"

namespace utils {
    namespace dnet {
        // forward cipher
        struct TLSCipher;

        namespace quic::crypto {
            // TODO(on-keyday): check each encryption model
            constexpr auto cipher_tag_length = 16;

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
                ByteLen processed_payload;

                ByteLen composition() const {
                    if (!head.adjacent(payload) || !payload.adjacent(tag)) {
                        return {};
                    }
                    return ByteLen{head.data, head.len + payload.len + tag.len};
                }
            };

            dnet_dll_export(bool) has_quic_ext();

            dnet_dll_export(bool) HKDF_Extract(ByteLen& initial,
                                               ByteLen secret, ByteLen salt);
            dnet_dll_export(bool) HKDF_Expand_label(WPacket& src, ByteLen output, ByteLen secret, ByteLen quic_label);

            // make_initial_secret generates initial secret from clientOrigDstID
            // WARNING(on-keyday): clientOrigDstID must be dstID field value of client sent at first packet
            dnet_dll_export(ByteLen) make_initial_secret(WPacket& src, std::uint32_t version, ByteLen clientOrigDstID, bool enc_client);

            dnet_dll_export(bool) make_keys_from_secret(WPacket& src, Keys& keys, std::uint32_t version, ByteLen secret, int hash_len);

            // encrypt_packet encrypts packet with specific cipher
            // version is QUIC packet header version field value
            // cipher is got from TLS.get_cipher or is TLSCipher{}
            // keys will replaced with generated keys
            // info is packet payload
            // secret is write secret
            // if cipher is TLSCipher{},secret will interpret as initial secret
            dnet_dll_export(bool) encrypt_packet(Keys& keys, std::uint32_t version, const TLSCipher& cipher, CryptoPacketInfo& info, ByteLen secret);

            // decrypt_packet decrypts packet with specific cipher
            // version is QUIC packet header version field value
            // cipher is got from TLS.get_cipher or is TLSCipher{}
            // keys will replaced with generated keys
            // info is packet payload
            // secret is write secret
            // if cipher is TLSCipher{},secret will interpret as initial secret
            dnet_dll_export(bool) decrypt_packet(Keys& keys, std::uint32_t version, const TLSCipher& cipher, CryptoPacketInfo& info, ByteLen secret);

            // internal functions
            bool cipher_initial_payload(ByteLen output, ByteLen payload, ByteLen head, ByteLen key, ByteLen iv_nonce, bool enc);
            bool generate_masks_aes_based(const byte* hp_key, byte* sample, byte* masks, bool is_aes256);
            bool generate_masks_chacha20_based(const byte* hp_key, byte* sample, byte* masks);
            // cipher_payload encrypts/decrypts payload with specific cipher
            bool cipher_payload(const TLSCipher& cipher, CryptoPacketInfo info, ByteLen key, ByteLen iv_nonce, bool enc);
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
