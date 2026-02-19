/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/dll/dllcpp.h>
#include <fnet/quic/crypto/crypto.h>
#include <helper/defer.h>
#include <fnet/tls/tls.h>
#include <fnet/quic/version.h>
#include <fnet/quic/types.h>
#include <fnet/quic/crypto/tls_suite.h>
#include <fnet/quic/crypto/internal.h>

namespace futils {
    namespace fnet {
        namespace quic::crypto {

            static constexpr auto err_packet_format = error::Error("CryptoPacket: QUIC packet format is not valid", error::Category::lib, error::fnet_quic_crypto_arg_error);
            static constexpr auto err_cipher_spec = error::Error("cipher spec is not AES based or ChaCha20 based", error::Category::lib, error::fnet_quic_crypto_arg_error);
            static constexpr auto err_aes_mask = error::Error("failed to generate header mask AES-based", error::Category::lib, error::fnet_quic_crypto_op_error);
            static constexpr auto err_cha_cha_mask = error::Error("failed to generate header mask ChaCha20-based", error::Category::lib, error::fnet_quic_crypto_arg_error);
            static constexpr auto err_not_enough_payload = error::Error("not enough CryptoPacket.processed_payload length", error::Category::lib, error::fnet_quic_crypto_arg_error);
            static constexpr auto err_too_short_payload = error::Error("not enough payload length. least 20 byte required", error::Category::lib, error::fnet_quic_crypto_arg_error);
            static constexpr auto err_unknown = error::Error("unknown packet parser error. library bug!", error::Category::lib, error::fnet_quic_implementation_bug);
            static constexpr auto err_decode_pn = error::Error("failed to decode packet number", error::Category::lib, error::fnet_quic_packet_number_error);
            static constexpr auto err_header_not_decrypted = error::Error("header is not decrypted. call decrypt_header() before call decrypt_payload()", error::Category::lib, error::fnet_quic_crypto_arg_error);

            tls::QUICCipherSuite judge_cipher(const tls::TLSCipher& cipher) {
                if (cipher == tls::TLSCipher{}) {
                    return tls::QUICCipherSuite::AES_128_GCM_SHA256;
                }
                return tls::to_suite(cipher.rfcname());
            }

            constexpr size_t sample_len = 16;

            error::Error gen_masks(byte* masks, view::rvec hp_key, const tls::TLSCipher& cipher, view::rvec sample) {
                auto suite = judge_cipher(cipher);
                if (suite == tls::QUICCipherSuite::Unsupported) {
                    return err_cipher_spec;
                }
                if (tls::is_AES_based(suite)) {
                    if (!generate_masks_aes_based(hp_key.data(), sample.data(), masks, tls::hash_len(suite) == 32)) {
                        return err_aes_mask;
                    }
                }
                else {
                    if (!generate_masks_chacha20_based(hp_key.data(), sample.data(), masks)) {
                        return err_cha_cha_mask;
                    }
                }
                return error::none;
            }

            // make nonce from iv and packet number
            // see https://tex2e.github.io/blog/protocol/quic-initial-packet-decrypt
            // see https://tex2e.github.io/rfc-translater/html/rfc9001.html#5-3--AEAD-Usage
            void make_nonce(view::rvec iv, byte (&nonce)[32], packetnum::Value packet_number) {
                binary::Buf<std::uint64_t, byte*> data{nonce + iv.size() - 8};
                data.write_be(packet_number.as_uint());
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

            error::Error protect_header(byte pnlen, view::rvec hp_key, const tls::TLSCipher& cipher, packet::CryptoPacket& packet) {
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

            std::pair<packetnum::WireVal, error::Error> unprotect_header(view::rvec hp_key, const tls::TLSCipher& cipher, packet::CryptoPacket& packet) {
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
                binary::reader r{pn_wire};
                auto [wireval, ok2] = packetnum::read(r, pnlen);
                if (!ok2) {
                    return {{}, err_decode_pn};
                }
                return {wireval, error::none};
            }

            fnet_dll_implement(error::Error) encrypt_packet(const KeyIV& keyiv, const HP& hp, const tls::TLSCipher& cipher, packet::CryptoPacket& packet) {
                if (packet.head_len < 1 || packet.src.size() < 1) {
                    return err_packet_format;
                }
                if (packet.payload_len() < 20) {
                    return err_too_short_payload;
                }
                auto flags = PacketFlags{packet.src[0]};
                auto pnlen = flags.packet_number_length();
                auto [parsed, ok] = packet.parse_pnknown(pnlen, authentication_tag_length);
                if (!ok) {
                    return err_packet_format;
                }
                // make nonce from iv and packet number
                byte nonce[32]{0};
                make_nonce(keyiv.iv(), nonce, packet.packet_number);
                // encrypt payload
                if (auto err = cipher_payload(cipher, parsed, keyiv.key(), view::rvec(nonce, keyiv.iv().size()), true)) {
                    return err;
                }
                // apply header protection
                return protect_header(pnlen, hp.hp(), cipher, packet);
            }

            fnet_dll_export(error::Error) decrypt_header(const HP& hp, const tls::TLSCipher& cipher, packet::CryptoPacket& packet, packetnum::Value largest_pn) {
                if (packet.head_len < 1 || packet.src.size() < 1) {
                    return err_packet_format;
                }
                if (packet.payload_len() < 20) {
                    return err_too_short_payload;
                }
                // apply header unprotection
                auto [pn_wire, err] = unprotect_header(hp.hp(), cipher, packet);
                if (err) {
                    return err;
                }
                // decode packet number
                auto ok = packetnum::decode(pn_wire, largest_pn.as_uint());
                if (!ok) {
                    return err_decode_pn;
                }
                packet.packet_number = *ok;
                return error::none;
            }

            fnet_dll_implement(error::Error) decrypt_payload(const KeyIV& keyiv, const tls::TLSCipher& cipher, packet::CryptoPacket& packet) {
                if (packet.head_len < 1 || packet.src.size() < 1) {
                    return err_packet_format;
                }
                if (packet.payload_len() < 20) {
                    return err_too_short_payload;
                }
                if (packet.packet_number == packetnum::infinity) {
                    return err_header_not_decrypted;
                }
                auto [pnknown, ok] = packet.parse_pnknown(PacketFlags{packet.src[0]}.packet_number_length(), authentication_tag_length);
                if (!ok) {
                    return err_unknown;
                }
                // make nonce from iv and packet number
                byte nonce[32]{0};
                make_nonce(keyiv.iv(), nonce, packet.packet_number);
                // decrypt payload
                return cipher_payload(cipher, pnknown, keyiv.key(), view::rvec(nonce, keyiv.iv().size()), false);
            }

            fnet_dll_export(error::Error) generate_retry_integrity_tag(view::wvec tag, view::rvec pseudo_packet, std::uint32_t version) {
                if (version == version_1) {
                    packet::CryptoPacketPnKnown info;
                    byte tmp = 0;
                    info.auth_tag = tag;
                    info.head = pseudo_packet;
                    info.protected_payload = view::wvec(&tmp, 0);
                    return cipher_payload({}, info,
                                          quic_v1_retry_integrity_tag_key.material,
                                          quic_v1_retry_integrity_tag_nonce.material, true);
                }
                return error::Error("unknown QUIC version", error::Category::lib, error::fnet_quic_version_error);
            }

        }  // namespace quic::crypto
    }      // namespace fnet
}  // namespace futils
