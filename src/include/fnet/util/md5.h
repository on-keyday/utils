/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>
#include <binary/buf.h>

namespace futils::md5 {
    // K for calculated by sin(i) * 2^32
    // clang-format off
    constexpr std::uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21E1CDE6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,

        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
        0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1,
        0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391,
    };
    // clang-format on

    // s for shift amount
    // clang-format off
    constexpr std::uint32_t s[64]={
        7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
        5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
        4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
        6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
    };
    // clang-format on

    constexpr std::uint32_t a0 = 0x67452301;
    constexpr std::uint32_t b0 = 0xefcdab89;
    constexpr std::uint32_t c0 = 0x98badcfe;
    constexpr std::uint32_t d0 = 0x10325476;

    struct MD5 {
       private:
        byte buffer[64]{};
        byte index = 0;
        std::uint32_t digest[4]{a0, b0, c0, d0};
        std::uint64_t initial_msg_length = 0;

        constexpr void do_update() {
            auto left_rotate = [](std::uint32_t x, std::uint32_t c) {
                return (x << c) | (x >> (32 - c));
            };
            std::uint32_t a = digest[0];
            std::uint32_t b = digest[1];
            std::uint32_t c = digest[2];
            std::uint32_t d = digest[3];

            std::uint32_t m[16] = {
                binary::read_from<std::uint32_t>(buffer + 0, false),
                binary::read_from<std::uint32_t>(buffer + 4, false),
                binary::read_from<std::uint32_t>(buffer + 8, false),
                binary::read_from<std::uint32_t>(buffer + 12, false),
                binary::read_from<std::uint32_t>(buffer + 16, false),
                binary::read_from<std::uint32_t>(buffer + 20, false),
                binary::read_from<std::uint32_t>(buffer + 24, false),
                binary::read_from<std::uint32_t>(buffer + 28, false),
                binary::read_from<std::uint32_t>(buffer + 32, false),
                binary::read_from<std::uint32_t>(buffer + 36, false),
                binary::read_from<std::uint32_t>(buffer + 40, false),
                binary::read_from<std::uint32_t>(buffer + 44, false),
                binary::read_from<std::uint32_t>(buffer + 48, false),
                binary::read_from<std::uint32_t>(buffer + 52, false),
                binary::read_from<std::uint32_t>(buffer + 56, false),
                binary::read_from<std::uint32_t>(buffer + 60, false),
            };
            for (std::uint32_t i = 0; i < 64; i++) {
                std::uint32_t f = 0;
                std::uint32_t g = 0;
                if (i < 16) {
                    f = (b & c) | ((~b) & d);
                    g = i;
                }
                else if (i < 32) {
                    f = (d & b) | ((~d) & c);
                    g = (5 * i + 1) % 16;
                }
                else if (i < 48) {
                    f = (b ^ c) ^ d;
                    g = (3 * i + 5) % 16;
                }
                else {
                    f = c ^ (b | (~d));
                    g = (7 * i) % 16;
                }
                auto u = f + a + K[i] + m[g];
                a = d;
                d = c;
                c = b;
                b = b + left_rotate(u, s[i]);
            }
            // add to digest
            digest[0] += a;
            digest[1] += b;
            digest[2] += c;
            digest[3] += d;
            // reset index
            index = 0;
        }

       public:
        constexpr void init() {
            digest[0] = a0;
            digest[1] = b0;
            digest[2] = c0;
            digest[3] = d0;
            index = 0;
            initial_msg_length = 0;
        }

        // md5 hash functions
        // https://tools.ietf.org/html/rfc1321
        // https://ja.wikipedia.org/wiki/MD5
        constexpr void update(view::rvec input) {
            size_t i = 0;
            for (;;) {
                for (; index < 64 && i < input.size(); i++, index++) {
                    buffer[index] = input[i];
                }
                if (index != 64) {
                    initial_msg_length += input.size() * 8;
                    return;
                }
                do_update();
            }
        }

        constexpr bool finish(view::wvec out) {
            if (out.size() < 16) {
                return false;
            }
            buffer[index] = 0x80;
            index++;
            if (index > 56) {
                for (; index < 64; index++) {
                    buffer[index] = 0;
                }
                do_update();
            }
            for (; index < 56; index++) {
                buffer[index] = 0;
            }
            binary::write_into(buffer + 56, initial_msg_length, false);
            do_update();
            for (auto i = 0; i < 4; i++) {
                binary::write_into(out.data() + (4 * i), digest[i], false);
            }
            return true;
        }
    };

    namespace test {
        constexpr bool check_md5() {
            MD5 md5;
            byte result[16]{};
            md5.finish(result);
            // hash of empty string
            byte empty_hash[] = {0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00,
                                 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98,
                                 0xec, 0xf8, 0x42, 0x7e};
            if (view::rvec(empty_hash) != result) {
                [](auto s...) { throw "error 1"; }(result[0], result[1], result[2], result[3],
                                                   result[4], result[5], result[6], result[7],
                                                   result[8], result[9], result[10], result[11],
                                                   result[12], result[13], result[14], result[15]);
            }

            // test hello world become below hash
            // 5e b6 3b bb e0 1e ee d0 93 cb 22 bb 8f 5a cd c3
            byte hello_world[11] = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
            byte hello_world_hash[] = {0x5e, 0xb6, 0x3b, 0xbb, 0xe0, 0x1e,
                                       0xee, 0xd0, 0x93, 0xcb, 0x22, 0xbb,
                                       0x8f, 0x5a, 0xcd, 0xc3};
            md5.init();
            md5.update(hello_world);
            md5.finish(result);
            if (view::rvec(hello_world_hash) != result) {
                [](auto... s) { throw "error 2"; }(result[0], result[1], result[2], result[3],
                                                   result[4], result[5], result[6], result[7],
                                                   result[8], result[9], result[10], result[11],
                                                   result[12], result[13], result[14], result[15]);
            }
            return true;
        }

        static_assert(check_md5());
    }  // namespace test

}  // namespace futils::md5
