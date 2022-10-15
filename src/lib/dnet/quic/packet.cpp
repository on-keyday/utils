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

namespace utils {
    namespace dnet {
        namespace quic {
            bool generate_masks(const byte* hp_key, byte* sample, int samplelen, byte* masks) {
                auto& c = ssldl;
                auto ctx = c.EVP_CIPHER_CTX_new_();
                if (!ctx) {
                    return false;
                }
                auto r = helper::defer([&] {
                    c.EVP_CIPHER_CTX_free_(ctx);
                });
                auto err = c.EVP_CipherInit_(ctx, c.EVP_aes_128_ecb_(), hp_key, nullptr, 1);
                if (err != 1) {
                    return false;
                }
                byte data[32]{};
                int data_len = 32;
                err = c.EVP_CipherUpdate_(ctx, data, &data_len, sample, samplelen);
                if (err != 1) {
                    return false;
                }
                memcpy(masks, data, 5);
                return true;
            }

            bool cipher_payload(ByteLen output,
                                ByteLen payload,
                                ByteLen head,
                                ByteLen key,
                                ByteLen iv_nonce,
                                bool enc) {
                auto ctx = ssldl.EVP_CIPHER_CTX_new_();
                if (!ctx) {
                    return false;
                }
                const auto c_ = helper::defer([&] {
                    ssldl.EVP_CIPHER_CTX_free_(ctx);
                });
                // set cipher
                if (!ssldl.EVP_CipherInit_ex_(ctx, ssldl.EVP_aes_128_gcm_(), nullptr, nullptr, nullptr, enc)) {
                    return false;
                }
                // set iv len
                if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_GCM_SET_IVLEN_, iv_nonce.len, nullptr)) {
                    return false;
                }
                // set key and iv
                if (!ssldl.EVP_CipherInit_ex_(ctx, nullptr, nullptr, key.data, iv_nonce.data, enc)) {
                    return false;
                }
                int outlen = 0;
                // set packet header as AAD
                if (!ssldl.EVP_CipherUpdate_(ctx, nullptr, &outlen, head.data, head.len)) {
                    return false;
                }
                // encrypt plaitext or decrypt ciphertext
                if (!ssldl.EVP_CipherUpdate_(ctx, output.data, &outlen, payload.data, payload.len - (enc ? 0 : 16))) {
                    return false;
                }
                if (!enc) {
                    // read MAC tag from end of payload
                    if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_GCM_SET_TAG_, 16, payload.data + payload.len - 16)) {
                        return false;
                    }
                }
                // finalize cipher
                if (!ssldl.EVP_CipherFinal_ex_(ctx, output.data, &outlen)) {
                    return false;
                }
                if (enc) {
                    // add MAC tag to end of payload
                    if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_GCM_GET_TAG_, 16, output.data + output.len - 16)) {
                        return false;
                    }
                }
                return true;
            }

            dnet_dll_implement(bool) decrypt_initial_packet(PacketInfo& info, bool enc_client) {
                InitialKeys keys;
                if (!info.dstID.valid() ||
                    !info.payload.enough(20) ||
                    !info.head.adjacent(info.payload)) {
                    return false;
                }
                if (!make_initial_keys(keys, info.dstID.data, info.dstID.len, enc_client)) {
                    return false;
                }
                auto sample_start = info.payload.data + 4;
                byte masks[5];
                if (!generate_masks(keys.hp.key, sample_start, 16, masks)) {
                    return false;
                }
                PacketFlags prot{info.head.data + 0};
                prot.protect(masks[0]);
                auto pn_length = prot.packet_number_length();
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                if (!info.processed_payload.enough(info.payload.len - pn_length - 16)) {
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
                return cipher_payload(
                    info.processed_payload,
                    ByteLen{info.payload.data + pn_length, info.payload.len - pn_length},
                    ByteLen{info.head.data + 0, info.head.len + pn_length},
                    ByteLen{keys.key.key, keys.key.size()},
                    ByteLen{nonce, keys.iv.size()}, false);
            }

            dnet_dll_implement(bool) encrypt_initial_packet(PacketInfo& info, bool enc_client) {
                InitialKeys keys;
                if (!info.dstID.valid() ||
                    !info.head.adjacent(info.payload)) {
                    return false;
                }
                if (!make_initial_keys(keys, info.dstID.data, info.dstID.len, enc_client)) {
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
                if (!cipher_payload(
                        info.processed_payload,
                        ByteLen{info.payload.data + pn_length, info.payload.len - pn_length},
                        ByteLen{info.head.data + 0, info.head.len + pn_length},
                        ByteLen{keys.key.key, keys.key.size()},
                        ByteLen{nonce, keys.iv.size()}, true)) {
                    return false;
                }
                auto sample_start = info.processed_payload.data + 4 - pn_length;
                byte masks[5];
                if (!generate_masks(keys.hp.key, sample_start, 16, masks)) {
                    return false;
                }
                prot.protect(masks[0]);
                for (size_t i = 0; i < pn_length; i++) {
                    info.payload.data[i] ^= masks[1 + i];
                }
                return true;
            }
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
