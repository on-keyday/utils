/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// sha1 - sha1 hash
// almost copied from commonlib
// use on websocket
#pragma once

#include <binary/reader.h>
#include <binary/buf.h>
#include <helper/pushbacker.h>
#include <core/sequencer.h>

namespace futils {
    namespace sha {
        namespace internal {
            constexpr void calc_sha1(std::uint32_t* h, const std::uint8_t* bits) {
                auto rol = [](unsigned int word, auto shift) {
                    return (word << shift) | (word >> (sizeof(word) * 8 - shift));
                };
                std::uint32_t w[80] = {0};
                std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
                for (auto i = 0; i < 80; i++) {
                    if (i < 16) {
                        binary::Buf<std::uint32_t> buf;
                        buf.from(bits + (i * 4));
                        w[i] = buf.read_be();
                    }
                    else {
                        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
                    }
                    std::uint32_t f = 0, k = 0;
                    if (i <= 19) {
                        f = (b & c) | ((~b) & d);
                        k = 0x5A827999;
                    }
                    else if (i <= 39) {
                        f = b ^ c ^ d;
                        k = 0x6ED9EBA1;
                    }
                    else if (i <= 59) {
                        f = (b & c) | (b & d) | (c & d);
                        k = 0x8F1BBCDC;
                    }
                    else {
                        f = b ^ c ^ d;
                        k = 0xCA62C1D6;
                    }
                    unsigned int tmp = rol(a, 5) + f + e + k + w[i];
                    e = d;
                    d = c;
                    c = rol(b, 30);
                    b = a;
                    a = tmp;
                }
                h[0] += a;
                h[1] += b;
                h[2] += c;
                h[3] += d;
                h[4] += e;
            }
        }  // namespace internal

        struct SHA1 {
           private:
            std::uint32_t hash[5];
            std::uint64_t total = 0;
            std::uint8_t buffer[64] = {};
            size_t buf_size = 0;

           public:
            constexpr SHA1() {
                reset();
            }

            void clear_buffer() {
                for (auto& b : buffer) {
                    b = 0;
                }
                buf_size = 0;
            }

            void reset() {
                hash[0] = 0x67452301;
                hash[1] = 0xEFCDAB89;
                hash[2] = 0x98BADCFE;
                hash[3] = 0x10325476;
                hash[4] = 0xC3D2E1F0;
                total = 0;
                clear_buffer();
            }

            template <class T>
            constexpr void update(T&& t) {
                auto seq = make_ref_seq(t);
                while (!seq.eos()) {
                    buffer[buf_size] = seq.current();
                    buf_size++;
                    if (buf_size == 64) {
                        total += 64 * 8;
                        internal::calc_sha1(hash, buffer);
                        clear_buffer();
                    }
                    seq.consume();
                }
            }

            template <class Out>
            constexpr void finalize(Out&& out) {
                auto flush_total = [&]() {
                    binary::Buf<std::uint64_t> total_buf;
                    total_buf.write_be(total);
                    total_buf.into_array(buffer + buf_size, 56);
                };
                buffer[buf_size] = 0x80;
                if (buf_size < 56) {
                    flush_total();
                    internal::calc_sha1(hash, buffer);
                }
                else {
                    internal::calc_sha1(hash, buffer);
                    clear_buffer();
                    flush_total();
                    internal::calc_sha1(hash, buffer);
                }
                for (auto h : hash) {
                    binary::Buf<std::uint32_t> buf;
                    buf.write_be(h);
                    for (auto k = 0; k < 4; k++) {
                        out.push_back(buf[k]);
                    }
                }
            }
        };

        template <class T, class Out>
        constexpr bool make_sha1(Sequencer<T>& seq, Out& out) {
            std::uint32_t hash[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
            using Buffer = helper::FixedPushBacker<std::uint8_t[64], 64>;
            size_t total = 0;
            bool intotal = false;
            auto read_ = [&]() {
                Buffer buf = {};
                auto i = 0;
                for (; i < 64; i++) {
                    if (seq.eos()) {
                        break;
                    }
                    buf.push_back(seq.current());
                    seq.consume();
                }
                return std::pair{buf, i};
            };
            binary::Buf<std::uint64_t> total_buf;
            auto flush_total = [&](auto& b) {
                total_buf.write_be(total);
                total_buf.into_array(b.buf, 56);
            };
            while (!seq.eos()) {
                auto [b, red] = read_();
                total += red * 8;
                if (red < 64) {
                    b.buf[red] = 0x80;
                    intotal = true;
                    if (red < 56) {
                        flush_total(b);
                        internal::calc_sha1(hash, b.buf);
                    }
                    else {
                        internal::calc_sha1(hash, b.buf);
                        b = {};
                        flush_total(b);
                        internal::calc_sha1(hash, b.buf);
                    }
                }
                else {
                    internal::calc_sha1(hash, b.buf);
                }
            }
            if (!intotal) {
                Buffer b = {};
                b.buf[0] = 0x80;
                flush_total(b);
                internal::calc_sha1(hash, b.buf);
            }
            for (auto h : hash) {
                binary::Buf<std::uint32_t> buf;
                buf.write_be(h);
                for (auto k = 0; k < 4; k++) {
                    out.push_back(buf[k]);
                }
            }
            return true;
        }

        template <class In, class Out>
        constexpr bool make_sha1(In&& in, Out& out) {
            auto seq = make_ref_seq(in);
            return make_sha1(seq, out);
        }
    }  // namespace sha

}  // namespace futils
