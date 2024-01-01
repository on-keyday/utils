/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/crypto/crypto.h>
#include <fnet/dll/lazy/ssldll.h>
#include <helper/defer.h>
#include <cstring>
#include <binary/buf.h>

namespace futils {
    namespace fnet {
        namespace quic::crypto {

            bool generate_masks_aes_based(const byte* hp_key, const byte* sample, byte* masks, bool is_aes256) {
                auto ctx = lazy::crypto::EVP_CIPHER_CTX_new_();
                if (!ctx) {
                    return false;
                }
                const auto r = helper::defer([&] {
                    lazy::crypto::EVP_CIPHER_CTX_free_(ctx);
                });
                auto err = lazy::crypto::EVP_CipherInit_(ctx, is_aes256 ? lazy::crypto::EVP_aes_256_ecb_() : lazy::crypto::EVP_aes_128_ecb_(), hp_key, nullptr, 1);
                if (err != 1) {
                    return false;
                }
                byte data[64]{};
                int data_len = 32;
                err = lazy::crypto::EVP_CipherUpdate_(ctx, data, &data_len, sample, 16);
                if (err != 1) {
                    return false;
                }
                memcpy(masks, data, 5);
                return true;
            }

            bool generate_masks_chacha20_based(const byte* hp_key, const byte* sample, byte* masks) {
                byte data[32];
                int outlen = 32;
                byte zeros[5]{0};
                if (lazy::crypto::bssl::sp::CRYPTO_chacha_20_.find()) {
                    auto counter = binary::Buf<std::uint32_t, const byte*>{sample}.read_le();
                    lazy::crypto::bssl::sp::CRYPTO_chacha_20_(data, zeros, 5, hp_key, sample + 4, counter);
                    memcpy(masks, zeros, 5);
                    return true;
                }
                auto ctx = lazy::crypto::EVP_CIPHER_CTX_new_();
                if (!ctx) {
                    return false;
                }
                auto r = helper::defer([&] {
                    lazy::crypto::EVP_CIPHER_CTX_free_(ctx);
                });
                if (!lazy::crypto::EVP_CipherInit_(ctx, lazy::crypto::ossl::sp::EVP_chacha20_(), hp_key, sample, true)) {
                    return false;
                }
                if (!lazy::crypto::EVP_CipherUpdate_(ctx, data, &outlen, zeros, 5)) {
                    return false;
                }
                memcpy(masks, data, 5);
                return true;
            }
        }  // namespace quic::crypto
    }      // namespace fnet
}  // namespace futils
