/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <dnet/dll/dllcpp.h>
#include <dnet/quic/crypto.h>
#include <dnet/dll/ssldll.h>
#include <helper/defer.h>
#include <dnet/tls.h>

namespace utils {
    namespace dnet {
        namespace quic::crypto {
            /*
            dnet_dll_implement(bool) encrypt_initial_packet(CryptoPacketInfo& info, bool enc_client) {
                InitialKeys keys;
                if (!info.clientDstID.valid() ||
                    !info.head.adjacent(info.payload) ||
                    !info.payload.adjacent(info.tag)) {
                    return false;
                }
                byte pool[200];
                WPacket tmp{{pool, 200}};
                if (!make_initial_keys(keys, tmp, info.clientDstID, enc_client)) {
                    return false;
                }
                PacketFlags prot{info.head.data + 0};
                auto pn_length = prot.packet_number_length();
                byte nonce[sizeof(keys.iv.key)]{0};
                for (auto i = 0; i < pn_length; i++) {
                    nonce[sizeof(nonce) - 1 - (pn_length - 1 - i)] = info.payload.data[i];
                }
                for (auto i = 0; i < keys.iv.size(); i++) {
                    nonce[i] ^= keys.iv.key[i];
                }
                auto cipher_text_len = info.payload.len - pn_length + 16;
                if (!info.processed_payload.enough(cipher_text_len)) {
                    return false;
                }
                if (!cipher_initial_payload(
                        info.processed_payload,
                        ByteLen{info.payload.data + pn_length, info.payload.len - pn_length},
                        ByteLen{info.head.data + 0, info.head.len + pn_length},
                        ByteLen{keys.key.key, keys.key.size()},
                        ByteLen{nonce, keys.iv.size()}, true)) {
                    return false;
                }
                auto sample_start = info.processed_payload.data + 4 - pn_length;
                byte masks[5];
                if (!generate_masks_aes_based(keys.hp.key, sample_start, masks, false)) {
                    return false;
                }
                prot.protect(masks[0]);
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                return true;
            }

            dnet_dll_implement(bool) decrypt_initial_packet(CryptoPacketInfo& info, bool enc_client) {
                InitialKeys keys;
                if (!info.clientDstID.valid() ||
                    !info.payload.enough(20) ||
                    !info.head.adjacent(info.payload)) {
                    return false;
                }
                byte pool[200];
                WPacket tmp{{pool, 200}};
                if (!make_initial_keys(keys, tmp, info.clientDstID, enc_client)) {
                    return false;
                }
                auto sample_start = info.payload.data + 4;
                byte masks[5];
                if (!generate_masks_aes_based(keys.hp.key, sample_start, masks, false)) {
                    return false;
                }
                PacketFlags prot{info.head.data + 0};
                prot.protect(masks[0]);
                auto pn_length = prot.packet_number_length();
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                if (!info.processed_payload.valid()) {
                    info.processed_payload = ByteLen{
                        info.payload.data + pn_length,
                        info.payload.len - pn_length - 16,
                    };
                }
                const auto plaintext_len = info.payload.len - pn_length - 16;
                if (!info.processed_payload.enough(plaintext_len)) {
                    return false;
                }
                byte nonce[sizeof(keys.iv.key)]{0};
                for (auto i = 0; i < pn_length; i++) {
                    // nonce generation
                    // see https://tex2e.github.io/blog/protocol/quic-initial-packet-decrypt
                    nonce[sizeof(nonce) - 1 - (pn_length - 1 - i)] = info.payload.data[i];
                }
                for (auto i = 0; i < keys.iv.size(); i++) {
                    nonce[i] ^= keys.iv.key[i];
                }
                return cipher_initial_payload(
                    info.processed_payload,
                    ByteLen{info.payload.data + pn_length, info.payload.len - pn_length},
                    ByteLen{info.head.data + 0, info.head.len + pn_length},
                    ByteLen{keys.key.key, keys.key.size()},
                    ByteLen{nonce, keys.iv.size()}, false);
            }
            */

            dnet_dll_implement(bool) encrypt_packet(Keys& keys, std::uint32_t version, const TLSCipher& cipher, CryptoPacketInfo& info, ByteLen secret) {
                if (!info.head.adjacent(info.payload)) {
                    return false;
                }
                const auto is_AES = cipher == TLSCipher{} || cipher.is_AES_based();
                const auto is_CHACHA20 = cipher.is_CHACHA20_based();
                if (!is_AES && !is_CHACHA20) {
                    return false;
                }
                bool is_AES256 = false;
                int hash_len = 0;
                if (is_CHACHA20) {
                    hash_len = 32;
                }
                else {
                    is_AES256 = cipher.is_algorithm(TLS_AES_256_GCM);
                    hash_len = is_AES256 ? 32 : 16;
                }
                byte pool[200]{};
                WPacket tmp{{pool, 200}};
                // make key,iv,hp
                if (!make_keys_from_secret(tmp, keys, version, secret, hash_len)) {
                    return false;
                }
                PacketFlags prot{info.head.data + 0};
                auto pn_length = prot.packet_number_length();
                auto cipher_text_len = info.payload.len - pn_length + cipher_tag_length;
                if (!info.processed_payload.enough(cipher_text_len)) {
                    return false;
                }
                // make nonce from iv and packet number
                byte nonce[32]{0};
                for (auto i = 0; i < pn_length; i++) {
                    nonce[keys.iv.len - 1 - (pn_length - 1 - i)] = info.payload.data[i];
                }
                for (auto i = 0; i < keys.iv.len; i++) {
                    nonce[i] ^= keys.iv.data[i];
                }
                // pack encrypt data
                CryptoPacketInfo pass;
                pass.head = {info.head.data, info.head.len + pn_length};
                pass.payload = {info.payload.data + pn_length, info.payload.len - pn_length};
                pass.processed_payload = info.processed_payload;
                pass.tag = info.tag;  // unused but set for debug
                // payload encryption
                auto res = cipher_payload(
                    cipher,
                    pass,
                    keys.key,
                    ByteLen{nonce, keys.iv.len}, true);
                if (!res) {
                    return false;
                }
                // header protection
                auto sample_start = info.processed_payload.data + 4 - pn_length;
                byte masks[5]{0};
                if (is_AES) {
                    if (!generate_masks_aes_based(keys.hp.data, sample_start, masks, is_AES256)) {
                        return false;
                    }
                }
                else {
                    if (!generate_masks_chacha20_based(keys.hp.data, sample_start, masks)) {
                        return false;
                    }
                }
                prot.protect(masks[0]);  // protect packet number length
                // protect packet number field
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                return true;
            }

            dnet_dll_implement(bool) decrypt_packet(Keys& keys, std::uint32_t version, const TLSCipher& cipher, CryptoPacketInfo& info, ByteLen secret) {
                if (!info.payload.enough(20) ||
                    !info.head.adjacent(info.payload) ||
                    !info.payload.adjacent(info.tag)) {
                    return false;
                }
                const auto is_AES = cipher == TLSCipher{} || cipher.is_AES_based();
                const auto is_CHACHA20 = cipher.is_CHACHA20_based();
                if (!is_AES && !is_CHACHA20) {
                    return false;
                }
                bool is_AES256 = false;
                int hash_len = 0;
                if (is_CHACHA20) {
                    hash_len = 32;
                }
                else {
                    is_AES256 = cipher.is_algorithm(TLS_AES_256_GCM);
                    hash_len = is_AES256 ? 32 : 16;
                }
                byte pool[200]{};
                WPacket tmp{{pool, 200}};
                // make key,iv,hp
                if (!make_keys_from_secret(tmp, keys, version, secret, hash_len)) {
                    return false;
                }
                PacketFlags prot{info.head.data + 0};
                // header unprotection
                auto sample_start = info.payload.data + 4;
                byte masks[5]{0};
                if (is_AES) {
                    if (!generate_masks_aes_based(keys.hp.data, sample_start, masks, is_AES256)) {
                        return false;
                    }
                }
                else {
                    if (!generate_masks_chacha20_based(keys.hp.data, sample_start, masks)) {
                        return false;
                    }
                }
                prot.protect(masks[0]);  // unprotect packet number length
                auto pn_length = prot.packet_number_length();
                // unprotect packet number field
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                // check output area len
                const auto plain_text_len = info.payload.len - pn_length - cipher_tag_length;
                if (!info.processed_payload.valid()) {
                    info.processed_payload = ByteLen{
                        info.payload.data + pn_length,
                        info.payload.len - pn_length - cipher_tag_length,
                    };
                }
                if (!info.processed_payload.enough(plain_text_len)) {
                    return false;
                }
                // make nonce from iv and packet number
                byte nonce[32]{0};
                for (auto i = 0; i < pn_length; i++) {
                    nonce[keys.iv.len - 1 - (pn_length - 1 - i)] = info.payload.data[i];
                }
                for (auto i = 0; i < keys.iv.len; i++) {
                    nonce[i] ^= keys.iv.data[i];
                }
                // pack decrypt data
                CryptoPacketInfo pass;
                pass.head = {info.head.data, info.head.len + pn_length};
                pass.payload = {info.payload.data + pn_length, info.payload.len - pn_length};
                pass.processed_payload = info.processed_payload;
                pass.tag = info.tag;
                // payload decryption
                return cipher_payload(
                    cipher,
                    pass,
                    keys.key,
                    ByteLen{nonce, keys.iv.len}, false);
            }
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
