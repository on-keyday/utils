/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <dnet/dll/dllcpp.h>
#include <dnet/quic/crypto/crypto.h>
#include <dnet/dll/ssldll.h>
#include <helper/defer.h>
#include <dnet/tls.h>
#include <dnet/quic/version.h>

namespace utils {
    namespace dnet {
        namespace quic::crypto {

            static constexpr auto err_packet_format = error::Error("CryptoPacketInfo: QUIC packet format is not valid", error::ErrorCategory::validationerr);
            static constexpr auto err_cipher_spec = error::Error("cipher spec is not AES based or CHACHA20 based", error::ErrorCategory::validationerr);
            static constexpr auto err_aes_mask = error::Error("failed to generate header mask AES-based", error::ErrorCategory::cryptoerr);
            static constexpr auto err_chacha_mask = error::Error("failed to generate header mask ChaCha20-based", error::ErrorCategory::cryptoerr);
            static constexpr auto err_not_enough_paylaod = error::Error("not enough CryptoPacketInfo.processed_payload length", error::ErrorCategory::validationerr);

            dnet_dll_implement(error::Error) encrypt_packet(Keys& keys, std::uint32_t version, const TLSCipher& cipher, CryptoPacketInfo& info, ByteLen secret) {
                if (!info.head.adjacent(info.payload)) {
                    return err_packet_format;
                }
                const auto is_AES = cipher == TLSCipher{} || cipher.is_AES_based();
                const auto is_CHACHA20 = cipher.is_CHACHA20_based();
                if (!is_AES && !is_CHACHA20) {
                    return err_cipher_spec;
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
                if (auto err = make_keys_from_secret(tmp, keys, version, secret, hash_len)) {
                    return err;
                }
                PacketFlags prot{*info.head.data};
                auto pn_length = prot.packet_number_length();
                auto cipher_text_len = info.payload.len - pn_length + cipher_tag_length;
                if (!info.processed_payload.enough(cipher_text_len)) {
                    return err_not_enough_paylaod;
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
                auto err = cipher_payload(
                    cipher,
                    pass,
                    keys.key,
                    ByteLen{nonce, keys.iv.len}, true);
                if (err) {
                    return err;
                }
                // header protection
                auto sample_start = info.processed_payload.data + 4 - pn_length;
                byte masks[5]{0};
                if (is_AES) {
                    if (!generate_masks_aes_based(keys.hp.data, sample_start, masks, is_AES256)) {
                        return err_aes_mask;
                    }
                }
                else {
                    if (!generate_masks_chacha20_based(keys.hp.data, sample_start, masks)) {
                        return err_chacha_mask;
                    }
                }
                info.head.data[0] = prot.protect(masks[0]);  // protect packet number length
                // protect packet number field
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                return error::none;
            }

            dnet_dll_implement(error::Error) decrypt_packet(Keys& keys, std::uint32_t version, const TLSCipher& cipher, CryptoPacketInfo& info, ByteLen secret) {
                if (!info.payload.enough(20) ||
                    !info.head.adjacent(info.payload) ||
                    !info.payload.adjacent(info.tag) ||
                    info.tag.len != cipher_tag_length) {
                    return err_packet_format;
                }
                const auto is_AES = cipher == TLSCipher{} || cipher.is_AES_based();
                const auto is_CHACHA20 = cipher.is_CHACHA20_based();
                if (!is_AES && !is_CHACHA20) {
                    return err_cipher_spec;
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
                if (auto err = make_keys_from_secret(tmp, keys, version, secret, hash_len)) {
                    return err;
                }
                // header unprotection
                auto sample_start = info.payload.data + 4;
                byte masks[5]{0};
                if (is_AES) {
                    if (!generate_masks_aes_based(keys.hp.data, sample_start, masks, is_AES256)) {
                        return err_aes_mask;
                    }
                }
                else {
                    if (!generate_masks_chacha20_based(keys.hp.data, sample_start, masks)) {
                        return err_chacha_mask;
                    }
                }
                info.head.data[0] = PacketFlags{info.head.data[0]}.protect(masks[0]);  // unprotect packet number length
                auto pn_length = PacketFlags{info.head.data[0]}.packet_number_length();
                // unprotect packet number field
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                // check output area len
                const auto plain_text_len = info.payload.len - pn_length;
                if (!info.processed_payload.valid()) {
                    info.processed_payload = ByteLen{
                        info.payload.data + pn_length,
                        info.payload.len - pn_length,
                    };
                }
                if (!info.processed_payload.enough(plain_text_len)) {
                    return err_not_enough_paylaod;
                }
                // make nonce from iv and packet number
                // see https://tex2e.github.io/blog/protocol/quic-initial-packet-decrypt
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

            dnet_dll_implement(error::Error) generate_retry_integrity_tag(Key<16>& tag, ByteLen pseduo_packet, std::uint32_t version) {
                if (version == version_1) {
                    CryptoPacketInfo info;
                    info.tag = {tag.key, tag.size()};
                    info.head = pseduo_packet;
                    byte tmp = 0;
                    info.payload = {&tmp, 0};
                    info.processed_payload = {&tmp, 0};
                    auto key = quic_v1_retry_integrity_tag_key;
                    auto nonce = quic_v1_retry_integrity_tag_nonce;
                    return cipher_payload({}, info, {key.key, key.size()}, {nonce.key, nonce.size()}, true);
                }
                return error::Error("unknown QUIC version", error::ErrorCategory::quicerr);
            }
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
