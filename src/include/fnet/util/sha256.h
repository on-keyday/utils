/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <view/iovec.h>

namespace futils::sha {

    class SHA256 {
       public:
        constexpr SHA256() {
            reset();
        }

        constexpr void update(view::rvec data) {
            size_t index = count & 0x3F;  // Bytes already processed in the buffer
            count += data.size();         // Update the total number of processed bytes
            size_t partLen = 64 - index;  // Remaining space in the buffer

            if (data.size() >= partLen) {
                // std::memcpy(&buffer[index], data, partLen);
                view::copy(view::wvec(&buffer[index], partLen), data);
                transform(buffer);
                size_t i = partLen;
                for (; i + 63 < data.size(); i += 64) {
                    transform(&data[i]);
                }
                index = 0;
                partLen = data.size() - i;
            }
            else {
                partLen = data.size();
            }

            // std::memcpy(&buffer[index], &data[data.size() - partLen], partLen);
            view::copy(view::wvec(&buffer[index], partLen), view::rvec(&data[data.size() - partLen], partLen));
        }

        constexpr bool finalize(view::wvec hash) {
            if (hash.size() < 32) {
                return false;
            }
            unsigned char bits[8];
            encode(bits, count * 8);
            byte tmp = 0x80;

            // Pad the data
            update(view::rvec(&tmp, 1));
            tmp = 0;
            while ((count & 0x3F) != 56) {
                update(view::rvec(&tmp, 1));
            }
            update(view::rvec(bits, 8));

            // Store the hash
            for (size_t i = 0; i < 8; ++i) {
                hash[i * 4] = (state[i] >> 24) & 0xFF;
                hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
                hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
                hash[i * 4 + 3] = state[i] & 0xFF;
            }
            return true;
        }
        constexpr void reset() {
            count = 0;
            state[0] = 0x6a09e667;
            state[1] = 0xbb67ae85;
            state[2] = 0x3c6ef372;
            state[3] = 0xa54ff53a;
            state[4] = 0x510e527f;
            state[5] = 0x9b05688c;
            state[6] = 0x1f83d9ab;
            state[7] = 0x5be0cd19;
        }

       private:
        uint32_t state[8]{};
        uint8_t buffer[64]{};
        uint64_t count = 0;

        static constexpr std::uint32_t k[] = {
            // clang-format off
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
            // clang-format on
        };

        // reference: https://qiita.com/tnakagawa/items/6321472098a2b73836ab

        constexpr std::uint32_t ROTR(std::uint32_t x, std::uint32_t n) {
            return (x >> n) | (x << (32 - n));
        }

        constexpr std::uint32_t SHR(std::uint32_t x, std::uint32_t n) {
            return x >> n;
        }

        constexpr std::uint32_t Ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
            return (x & y) ^ (~x & z);
        }

        constexpr std::uint32_t Maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
            return (x & y) ^ (x & z) ^ (y & z);
        }

        constexpr std::uint32_t SIGMA0(std::uint32_t x) {
            return ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22);
        }

        constexpr std::uint32_t SIGMA1(std::uint32_t x) {
            return ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25);
        }

        constexpr std::uint32_t sigma0(std::uint32_t x) {
            return ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3);
        }

        constexpr std::uint32_t sigma1(std::uint32_t x) {
            return ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10);
        }

        constexpr void transform(const unsigned char* block) {
            uint32_t w[64];
            for (size_t i = 0; i < 16; ++i) {
                auto p = i * 4;
                w[i] = (block[p] << 24) | (block[p + 1] << 16) |
                       (block[p + 2] << 8) | block[p + 3];
            }
            for (size_t i = 16; i < 64; ++i) {
                w[i] = (sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16]);
            }

            uint32_t a = state[0];
            uint32_t b = state[1];
            uint32_t c = state[2];
            uint32_t d = state[3];
            uint32_t e = state[4];
            uint32_t f = state[5];
            uint32_t g = state[6];
            uint32_t h = state[7];

            for (size_t i = 0; i < 64; ++i) {
                auto t1 = (h + SIGMA1(e) + Ch(e, f, g) + k[i] + w[i]);
                auto t2 = (SIGMA0(a) + Maj(a, b, c));
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }

            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
        }

        constexpr void encode(unsigned char* output, uint64_t input) {
            for (size_t i = 0; i < 8; ++i) {
                output[i] = (input >> (56 - i * 8)) & 0xFF;
            }
        }
    };

    namespace test {
        constexpr auto test_sha256() {
            byte target_hash[32]{}, generated_hash[32]{};
            auto str_to_hash = [&](const char* h) {
                auto len = futils::strlen(h);
                for (size_t i = 0; i < len; i += 2) {
                    auto c = h[i];
                    auto c2 = h[i + 1];
                    byte b = 0;
                    if (c >= '0' && c <= '9') {
                        b = c - '0';
                    }
                    else {
                        b = c - 'a' + 10;
                    }
                    b <<= 4;
                    if (c2 >= '0' && c2 <= '9') {
                        b |= c2 - '0';
                    }
                    else {
                        b |= c2 - 'a' + 10;
                    }
                    target_hash[i / 2] = b;
                }
            };
            constexpr auto test_data = "The quick brown fox jumps over the lazy dog";
            auto test_hash = [&](const char* data, const char* hash) {
                str_to_hash(hash);
                byte data_[100]{};
                auto len = futils::strlen(data);
                for (size_t i = 0; i < len; i++) {
                    data_[i] = data[i];
                }
                SHA256 sha;
                sha.update(view::rvec(data_, len));
                sha.finalize(generated_hash);
                if (view::rvec(target_hash, 32) != view::rvec(generated_hash, 32)) {
                    [](auto... e) {
                        throw "hash mismatch";
                    }(sha, target_hash[0], generated_hash[0], target_hash[1], generated_hash[1], target_hash[2], generated_hash[2]);
                }
            };
            test_hash("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
            test_hash("a", "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
            test_hash("hello", "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
            test_hash("there are data that has 57 byte data. have you ever seen?", "62c5450578bb79b7612e5debf3940d098cf81814401a799a812478c7c3279543");
            test_hash("there are data that has 56 byte data. you have ever seen", "1f9a7a537f286778220b3b945566b84982dc1552519f9af91d6adc11a7f760ab");
            return true;
        }

        static_assert(test_sha256());

    }  // namespace test

}  // namespace futils::sha
