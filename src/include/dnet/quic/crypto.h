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

        namespace quic {

            struct CryptoPacketInfo {
                // head is header of QUIC packet without packet number
                ByteLen head;
                // payload is payload of QUIC packet including packet number
                // payload.data must be adjacent to head.data
                ByteLen payload;
                // clientDstID is client destination connection ID of QUIC
                ByteLen clientDstID;
                // processed_payload is encrypted/decrypted payload without packet number
                ByteLen processed_payload;
            };

            dnet_dll_export(bool) has_quic_ext();

            dnet_dll_export(bool) HKDF_Extract(ByteLen& initial,
                                               ByteLen secret, ByteLen salt);
            dnet_dll_export(bool) HKDF_Expand_label(WPacket& src, ByteLen output, ByteLen secret, ByteLen quic_label);

            dnet_dll_export(bool) make_initial_keys(InitialKeys& key, WPacket& src, ByteLen clientConnID, bool enc_client);

            // encrypt_initial_packet encrypts InitialPacket inPacketInfo
            // enc_client represents whether encryption perspective is client
            // info.head and info.payload must be adjacent in binary
            // encrypted results would store into info.processed_payload
            // info.dstID is client destination connection ID
            // that is, if client, use self-generated dstID, else if server, use dstID from remote
            dnet_dll_export(bool) encrypt_initial_packet(CryptoPacketInfo& info, bool enc_client);

            // decrypt_initial_packet decrypts InitialPacket inPacketInfo
            // enc_client represents whether encryption perspective is client
            // info.head and info.payload must be adjucent in binary
            // encrypted results would store into info.processed_payload
            // if info.processed_payload is invalid,
            // this function make info.processed_payload from info.payload with exact offset provided
            // by packet protection decryption and will overwrite info.payload area with decrypted data
            dnet_dll_export(bool) decrypt_initial_packet(CryptoPacketInfo& info, bool enc_client);

            // make_keys_from_secret makes
            // key,initial vector,header protection key,and key update secret for QUIC
            // hash_len is byte length of hash
            dnet_dll_export(bool) make_keys_from_secret(Keys& keys, WPacket& src, ByteLen secret, int hash_len);

            // cipher_payload encrypts/decrypts payload with specific cipher
            dnet_dll_export(bool) cipher_payload(
                const TLSCipher& cipher, ByteLen output, ByteLen payload,
                ByteLen head, ByteLen key, ByteLen iv_nonce, bool enc);

            dnet_dll_export(bool) encrypt_packet(const TLSCipher& cipher, Keys& keys, CryptoPacketInfo& info, ByteLen secret);

            dnet_dll_export(bool) decrypt_packet(const TLSCipher& cipher, Keys& keys, CryptoPacketInfo& info, ByteLen secret);

            // internal functions
            bool cipher_initial_payload(ByteLen output,
                                        ByteLen payload,
                                        ByteLen head,
                                        ByteLen key,
                                        ByteLen iv_nonce,
                                        bool enc);
            bool generate_masks_aes_based(const byte* hp_key, byte* sample, byte* masks, bool is_aes256);
            bool generate_masks_chacha20_based(const byte* hp_key, byte* sample, byte* masks);
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
