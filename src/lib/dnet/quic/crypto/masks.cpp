/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/quic/crypto/crypto.h>
#include <dnet/dll/ssldll.h>
#include <helper/defer.h>
#include <cstring>
#include <endian/buf.h>

namespace utils {
    namespace dnet {
        namespace quic::crypto {

            bool generate_masks_aes_based(const byte* hp_key, byte* sample, byte* masks, bool is_aes256) {
                auto& c = ssldl;
                auto ctx = c.EVP_CIPHER_CTX_new_();
                if (!ctx) {
                    return false;
                }
                const auto r = helper::defer([&] {
                    c.EVP_CIPHER_CTX_free_(ctx);
                });
                auto err = c.EVP_CipherInit_(ctx, is_aes256 ? c.EVP_aes_256_ecb_() : c.EVP_aes_128_ecb_(), hp_key, nullptr, 1);
                if (err != 1) {
                    return false;
                }
                byte data[64]{};
                int data_len = 32;
                err = c.EVP_CipherUpdate_(ctx, data, &data_len, sample, 16);
                if (err != 1) {
                    return false;
                }
                memcpy(masks, data, 5);
                return true;
            }

            bool generate_masks_chacha20_based(const byte* hp_key, byte* sample, byte* masks) {
                byte data[32];
                int outlen = 32;
                byte zeros[5]{0};
                if (ssldl.CRYPTO_chacha_20_) {
                    auto counter = endian::Buf<std::uint32_t, byte*>{sample}.read_le();
                    ssldl.CRYPTO_chacha_20_(data, zeros, 5, hp_key, sample + 4, counter);
                    memcpy(masks, zeros, 5);
                    return true;
                }
                auto ctx = ssldl.EVP_CIPHER_CTX_new_();
                if (!ctx) {
                    return false;
                }
                auto r = helper::defer([&] {
                    ssldl.EVP_CIPHER_CTX_free_(ctx);
                });
                if (!ssldl.EVP_CipherInit_(ctx, ssldl.EVP_chacha20_(), hp_key, sample, true)) {
                    return false;
                }
                if (!ssldl.EVP_CipherUpdate_(ctx, data, &outlen, zeros, 5)) {
                    return false;
                }
                memcpy(masks, data, 5);
                return true;
            }
        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
