/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/ssldll.h>
#include <dnet/tls.h>
#include <helper/defer.h>
#include <dnet/quic/crypto/crypto.h>
#include <dnet/storage.h>
#include <dnet/quic/crypto/internal.h>

namespace utils {
    namespace dnet {
        namespace quic::crypto {

            error::Error cipher_payload_impl(
                const ssl_import::EVP_CIPHER* meth,
                packet::CryptoPacketPnKnown& info,
                view::rvec key,
                view::rvec iv_nonce,
                bool enc) {
                auto ctx = ssldl.EVP_CIPHER_CTX_new_();
                auto r = helper::defer([&] {
                    ssldl.EVP_CIPHER_CTX_free_(ctx);
                });
                // set cipher
                if (!ssldl.EVP_CipherInit_ex_(ctx, meth, nullptr, nullptr, nullptr, enc)) {
                    return error::Error("failed to initialize cipher context", error::ErrorCategory::cryptoerr);
                }
                // set iv len
                if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_AEAD_SET_IVLEN_, iv_nonce.size(), nullptr)) {
                    return error::Error("failed to set iv len", error::ErrorCategory::cryptoerr);
                }
                // set key and iv
                if (!ssldl.EVP_CipherInit_ex_(ctx, nullptr, nullptr, key.data(), iv_nonce.data(), enc)) {
                    return error::Error("failed to set key and iv len", error::ErrorCategory::cryptoerr);
                }
                int outlen = 0;
                // set packet header as AAD
                if (!ssldl.EVP_CipherUpdate_(ctx, nullptr, &outlen, info.head.data(), info.head.size())) {
                    return error::Error("failed to set QUIC packet header as AAD", error::ErrorCategory::cryptoerr);
                }
                auto output = info.protected_payload;
                // encrypt plaitext or decrypt ciphertext
                if (!ssldl.EVP_CipherUpdate_(ctx, output.data(), &outlen,
                                             info.protected_payload.data(),
                                             info.protected_payload.size())) {
                    return error::Error(enc ? "failed to encrypt QUIC packet" : "failed to decrypt QUIC packet", error::ErrorCategory::cryptoerr);
                }
                if (!enc) {
                    // read MAC tag from end of payload
                    if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_AEAD_SET_TAG_, info.auth_tag.size(), info.auth_tag.data())) {
                        return error::Error("failed to set MAC tag", error::ErrorCategory::cryptoerr);
                    }
                }
                // finalize cipher
                if (!ssldl.EVP_CipherFinal_ex_(ctx, info.protected_payload.data(), &outlen)) {
                    return error::Error(enc ? "failed to verify encryption" : "failed to verify decryption", error::ErrorCategory::cryptoerr);
                }
                if (enc) {
                    // add MAC tag to end of payload
                    if (!ssldl.EVP_CIPHER_CTX_ctrl_(ctx, ssl_import::EVP_CTRL_AEAD_GET_TAG_, info.auth_tag.size(),
                                                    info.auth_tag.data())) {
                        return error::Error("failed to get MAC tag", error::ErrorCategory::cryptoerr);
                    }
                }
                return error::none;
            }

            error::Error boringssl_chacha20_poly1305_cipher(
                const TLSCipher& cipher, packet::CryptoPacketPnKnown& info,
                view::rvec key, view::rvec iv_nonce, bool enc) {
                auto ctx = ssldl.EVP_AEAD_CTX_new_(ssldl.EVP_aead_chacha20_poly1305_(), key.data(), key.size(), 16);
                if (!ctx) {
                    return error::Error("unable to get cipher context of chacha20_poly1305", error::ErrorCategory::cryptoerr);
                }
                const auto r = helper::defer([&] {
                    ssldl.EVP_AEAD_CTX_free_(ctx);
                });
                if (enc) {
                    // in place encryption
                    auto plain_payload = info.protected_payload;
                    size_t outlen = 0;
                    size_t outtaglen = 16;
                    auto res = (bool)ssldl.EVP_AEAD_CTX_seal_scatter_(
                        ctx, info.protected_payload.data(), info.auth_tag.data(),
                        &outtaglen, info.auth_tag.size(),
                        iv_nonce.data(), iv_nonce.size(), plain_payload.data(), plain_payload.size(),
                        nullptr, 0, info.head.data(), info.head.size());
                    if (!res) {
                        return error::Error("encrypt payload with chacha20_poly1305 failed");
                    }
                }
                else {
                    auto plain_payload = info.protected_payload;
                    // in place decryption
                    auto res = (bool)ssldl.EVP_AEAD_CTX_open_gather_(
                        ctx, plain_payload.data(),
                        iv_nonce.data(), iv_nonce.size(),
                        info.protected_payload.data(), info.protected_payload.size(),
                        info.auth_tag.data(), info.auth_tag.size(),
                        info.head.data(), info.head.size());
                    if (!res) {
                        return error::Error("decrypt payload with chacha20_poly1305 failed");
                    }
                }
                return error::none;
            }

            struct CipherSuiteError {
                flex_storage method;

                void error(auto&& pb) {
                    helper::appends(pb, "unknown cipher suite: ", method);
                }
            };

            dnet_dll_implement(error::Error) cipher_payload(const TLSCipher& cipher, packet::CryptoPacketPnKnown& info, view::rvec key, view::rvec iv_nonce, bool enc) {
                if (cipher == TLSCipher{}) {
                    // initial packet
                    return cipher_payload_impl(ssldl.EVP_aes_128_gcm_(), info, key, iv_nonce, enc);
                }
                auto meth = ssldl.EVP_get_cipherbynid_(cipher.nid());
                if (!meth) {
                    if (tls::is_CHACHA20_based(judge_cipher(cipher)) && is_boringssl_crypto()) {
                        return boringssl_chacha20_poly1305_cipher(cipher, info, key, iv_nonce, enc);
                    }
                    return CipherSuiteError{view::rvec(cipher.rfcname())};
                }
                return cipher_payload_impl(meth, info, key, iv_nonce, enc);
            }
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
