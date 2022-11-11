/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/ssldll.h>
#include <dnet/bytelen.h>
#include <dnet/tls.h>
#include <helper/defer.h>
#include <dnet/quic/crypto.h>

namespace utils {
    namespace dnet {
        namespace quic::crypto {

            bool cipher_payload_impl(
                const ssl_import::EVP_CIPHER* meth,
                CryptoPacketInfo& info,
                ByteLen key,
                ByteLen iv_nonce,
                bool enc) {
                auto ctx = ssldl.EVP_CIPHER_CTX_new_();
                auto r = helper::defer([&] {
                    ssldl.EVP_CIPHER_CTX_free_(ctx);
                });
                // set cipher
                if (!ssldl.EVP_CipherInit_ex_(ctx, meth, nullptr, nullptr, nullptr, enc)) {
                    return false;
                }
                // set iv len
                if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_AEAD_SET_IVLEN_, iv_nonce.len, nullptr)) {
                    return false;
                }
                // set key and iv
                if (!ssldl.EVP_CipherInit_ex_(ctx, nullptr, nullptr, key.data, iv_nonce.data, enc)) {
                    return false;
                }
                int outlen = 0;
                // set packet header as AAD
                if (!ssldl.EVP_CipherUpdate_(ctx, nullptr, &outlen, info.head.data, info.head.len)) {
                    return false;
                }
                // encrypt plaitext or decrypt ciphertext
                if (!ssldl.EVP_CipherUpdate_(ctx, info.processed_payload.data, &outlen, info.payload.data, info.payload.len)) {
                    return false;
                }
                if (!enc) {
                    // read MAC tag from end of payload
                    if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_AEAD_SET_TAG_, info.tag.len, info.tag.data)) {
                        return false;
                    }
                }
                // finalize cipher
                if (!ssldl.EVP_CipherFinal_ex_(ctx, info.processed_payload.data, &outlen)) {
                    return false;
                }
                if (enc) {
                    // add MAC tag to end of payload
                    if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_AEAD_GET_TAG_, 16,
                                                    info.processed_payload.data + info.payload.len)) {
                        return false;
                    }
                }
                return true;
            }

            bool boringssl_chacha20_poly1305_cipher(
                const TLSCipher& cipher, CryptoPacketInfo& info,
                ByteLen key, ByteLen iv_nonce, bool enc) {
                auto ctx = ssldl.EVP_AEAD_CTX_new_(ssldl.EVP_aead_chacha20_poly1305_(), key.data, key.len, 16);
                if (!ctx) {
                    return false;
                }
                const auto r = helper::defer([&] {
                    ssldl.EVP_AEAD_CTX_free_(ctx);
                });
                auto output = info.processed_payload;
                if (enc) {
                    size_t outlen = 0;
                    size_t outtaglen = 16;
                    return (bool)ssldl.EVP_AEAD_CTX_seal_scatter_(
                        ctx, output.data, output.data + output.len - 16,
                        &outtaglen, output.len,
                        iv_nonce.data, iv_nonce.len, info.payload.data, info.payload.len,
                        nullptr, 0, info.head.data, info.head.len);
                }
                else {
                    return (bool)ssldl.EVP_AEAD_CTX_open_gather_(
                        ctx, output.data,
                        iv_nonce.data, iv_nonce.len,
                        info.payload.data, info.payload.len, info.tag.data, info.tag.len,
                        info.head.data, info.head.len);
                }
            }

            bool cipher_payload(const TLSCipher& cipher, CryptoPacketInfo info, ByteLen key, ByteLen iv_nonce, bool enc) {
                if (cipher == TLSCipher{}) {
                    // initial packet
                    return cipher_payload_impl(ssldl.EVP_aes_128_gcm_(), info, key, iv_nonce, enc);
                }
                auto meth = ssldl.EVP_get_cipherbynid_(cipher.nid());
                if (!meth) {
                    if (cipher.is_algorithm(TLS_CHACHA20_POLY1305) && is_boringssl_crypto()) {
                        return boringssl_chacha20_poly1305_cipher(cipher, info, key, iv_nonce, enc);
                    }
                    return false;
                }
                return cipher_payload_impl(meth, info, key, iv_nonce, enc);
            }
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
