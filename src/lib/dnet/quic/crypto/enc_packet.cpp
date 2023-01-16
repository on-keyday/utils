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
#include <dnet/quic/types.h>
#include <dnet/quic/crypto/tls_suite.h>
#include <dnet/quic/crypto/internal.h>

namespace utils {
    namespace dnet {
        namespace quic::crypto {

            static constexpr auto err_packet_format = error::Error("CryptoPacket: QUIC packet format is not valid", error::ErrorCategory::validationerr);
            static constexpr auto err_cipher_spec = error::Error("cipher spec is not AES based or CHACHA20 based", error::ErrorCategory::validationerr);
            static constexpr auto err_aes_mask = error::Error("failed to generate header mask AES-based", error::ErrorCategory::cryptoerr);
            static constexpr auto err_chacha_mask = error::Error("failed to generate header mask ChaCha20-based", error::ErrorCategory::cryptoerr);
            static constexpr auto err_not_enough_paylaod = error::Error("not enough CryptoPacket.processed_payload length", error::ErrorCategory::validationerr);
            static constexpr auto err_unknown = error::Error("unknown packet parser error. library bug!", error::ErrorCategory::dneterr);
            static constexpr auto err_decode_pn = error::Error("failed to decode packet number", error::ErrorCategory::quicerr);

            tls::TLSSuite judge_cipher(const TLSCipher& cipher) {
                if (cipher == TLSCipher{}) {
                    return tls::TLSSuite::AES_128_GCM_SHA256;
                }
                return tls::to_suite(cipher.rfcname());
            }

            constexpr size_t sample_len = 16;

            error::Error gen_masks(byte* masks, view::rvec hp_key, const TLSCipher& cipher, view::rvec sample) {
                auto suite = judge_cipher(cipher);
                if (suite == tls::TLSSuite::Unsupported) {
                    return err_cipher_spec;
                }
                if (tls::is_AES_based(suite)) {
                    if (!generate_masks_aes_based(hp_key.data(), sample.data(), masks, tls::hash_len(suite) == 32)) {
                        return err_aes_mask;
                    }
                }
                else {
                    if (!generate_masks_chacha20_based(hp_key.data(), sample.data(), masks)) {
                        return err_chacha_mask;
                    }
                }
                return error::none;
            }

            // make nonce from iv and packet number
            // see https://tex2e.github.io/blog/protocol/quic-initial-packet-decrypt
            // see https://tex2e.github.io/rfc-translater/html/rfc9001.html#5-3--AEAD-Usage
            void make_nonce(view::rvec iv, byte (&nonce)[32], packetnum::Value packet_number) {
                endian::Buf<std::uint64_t, byte*> data{nonce + iv.size() - 8};
                data.write_be(packet_number);
                for (auto i = 0; i < iv.size(); i++) {
                    nonce[i] ^= iv[i];
                }
            }

            void apply_hp_pn_mask(byte* mask, view::wvec pn_wire) {
                for (auto& w : pn_wire) {
                    w ^= *mask;
                    mask++;
                }
            }

            void apply_hp_first_byte_mask(byte mask, byte* first_byte) {
                *first_byte = PacketFlags{*first_byte}.protect(mask);
            }

            error::Error protect_header(byte pnlen, view::rvec hp_key, const TLSCipher& cipher, packet::CryptoPacket& packet) {
                auto [encrypted, ok1] = packet.parse(sample_len, authentication_tag_length);
                if (!ok1) {
                    return err_unknown;
                }
                // generate header protection mask
                byte masks[5]{0};
                auto err = gen_masks(masks, hp_key, cipher, encrypted.sample);
                if (err) {
                    return err;
                }
                auto pn_wire = encrypted.protected_payload.substr(0, pnlen);
                if (pn_wire.size() != pnlen) {
                    return err_unknown;
                }
                // apply protection
                apply_hp_first_byte_mask(masks[0], &encrypted.head[0]);
                apply_hp_pn_mask(masks + 1, pn_wire);
                return error::none;
            }

            std::pair<packetnum::WireVal, error::Error> unprotect_header(view::rvec hp_key, const TLSCipher& cipher, packet::CryptoPacket& packet) {
                auto [encrypted, ok1] = packet.parse(sample_len, authentication_tag_length);
                if (!ok1) {
                    return {{}, err_packet_format};
                }
                // generate header protection mask
                byte masks[5]{0};
                auto err = gen_masks(masks, hp_key, cipher, encrypted.sample);
                if (err) {
                    return {{}, err};
                }
                // apply unprotection for first_byte
                apply_hp_first_byte_mask(masks[0], &encrypted.head[0]);
                // get packet_number field value
                auto pnlen = PacketFlags{encrypted.head[0]}.packet_number_length();
                auto pn_wire = encrypted.protected_payload.substr(0, pnlen);
                if (pn_wire.size() != pnlen) {
                    return {{}, err_packet_format};
                }
                // apply unprotection for packet_number field
                apply_hp_pn_mask(masks + 1, pn_wire);
                io::reader r{pn_wire};
                auto [wireval, ok2] = packetnum::read(r, pnlen);
                if (!ok2) {
                    return {{}, err_decode_pn};
                }
                return {wireval, error::none};
            }

            dnet_dll_implement(error::Error) encrypt_packet(const Keys& keys, const TLSCipher& cipher, packet::CryptoPacket& packet) {
                if (packet.head_len < 1 || packet.src.size() < 1) {
                    return err_packet_format;
                }
                auto flags = PacketFlags{packet.src[0]};
                auto pnlen = flags.packet_number_length();
                auto [parsed, ok] = packet.parse_pnknown(pnlen, authentication_tag_length);
                if (!ok) {
                    return err_packet_format;
                }
                // make nonce from iv and packet number
                byte nonce[32]{0};
                make_nonce(keys.iv(), nonce, packet.packet_number);
                // encrypt payload
                if (auto err = cipher_payload(cipher, parsed, keys.key(), view::rvec(nonce, keys.iv().size()), true)) {
                    return err;
                }
                // apply header protection
                return protect_header(pnlen, keys.hp(), cipher, packet);
            }

            dnet_dll_implement(error::Error) decrypt_packet(const Keys& keys, const TLSCipher& cipher, packet::CryptoPacket& packet, size_t largest_pn) {
                if (packet.head_len < 1 || packet.src.size() < 1) {
                    return err_packet_format;
                }
                // apply header unprotection
                auto [pn_wire, err] = unprotect_header(keys.hp(), cipher, packet);
                if (err) {
                    return err;
                }
                auto [pnknown, ok] = packet.parse_pnknown(pn_wire.len, authentication_tag_length);
                if (!ok) {
                    return err_unknown;
                }
                // decode packet number
                std::tie(packet.packet_number, ok) = packetnum::decode(pn_wire, largest_pn);
                if (!ok) {
                    return err_decode_pn;
                }
                // make nonce from iv and packet number
                byte nonce[32]{0};
                make_nonce(keys.iv(), nonce, packet.packet_number);
                // decrypt payload
                return cipher_payload(cipher, pnknown, keys.key(), view::rvec(nonce, keys.iv().size()), false);
            }

            dnet_dll_export(error::Error) generate_retry_integrity_tag(view::wvec tag, view::rvec pseduo_packet, std::uint32_t version) {
                if (version == version_1) {
                    packet::CryptoPacketPnKnown info;
                    byte tmp = 0;
                    info.auth_tag = tag;
                    info.head = pseduo_packet;
                    info.protected_payload = view::wvec(&tmp, 0);
                    auto key = quic_v1_retry_integrity_tag_key;
                    auto nonce = quic_v1_retry_integrity_tag_nonce;
                    return cipher_payload({}, info,
                                          quic_v1_retry_integrity_tag_key.key,
                                          quic_v1_retry_integrity_tag_nonce.key, true);
                }
                return error::Error("unknown QUIC version", error::ErrorCategory::quicerr);
            }

        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
