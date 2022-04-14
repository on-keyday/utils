/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// sha1 - sha1 hash
// almost copied from commonlib
// use on websocket
#pragma once

#include "../endian/reader.h"
#include "../helper/pushbacker.h"

namespace utils {
    namespace net {
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
                            w[i] = endian::to_network_us<std::uint32_t>(&bits[i * 4]);
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

            template <class T, class Out>
            constexpr bool make_sha1(Sequencer<T>& t, Out& out) {
                std::uint32_t hash[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
                endian::Reader<buffer_t<std::remove_reference_t<T>&>> r{t.buf};
                using Buffer = helper::FixedPushBacker<std::uint8_t[64], 64>;
                size_t total = 0;
                bool intotal = false;
                r.seq.rptr = t.rptr;
                while (!r.seq.eos()) {
                    Buffer b;
                    auto red = r.template read_seq<std::uint8_t, decltype(b), 1, 0, true>(b, 64, 0);
                    total += red * 8;
                    std::uint64_t* ptr = reinterpret_cast<std::uint64_t*>(b.buf);
                    if (red < 64) {
                        b.buf[red] = 0x80;
                        intotal = true;
                        if (red < 56) {
                            ptr[7] = endian::to_network(&total);
                            internal::calc_sha1(hash, b.buf);
                        }
                        else {
                            internal::calc_sha1(hash, b.buf);
                            ::memset(b.buf, 0, 64);
                            ptr[7] = endian::to_network(&total);
                            internal::calc_sha1(hash, b.buf);
                        }
                    }
                    else {
                        internal::calc_sha1(hash, b.buf);
                    }
                }
                if (!intotal) {
                    Buffer b;
                    std::uint64_t* ptr = reinterpret_cast<std::uint64_t*>(b.buf);
                    b.buf[0] = 0x80;
                    ptr[7] = endian::to_network(&total);
                    internal::calc_sha1(hash, b.buf);
                }
                for (auto h : hash) {
                    auto be = endian::to_network(&h);
                    for (auto k = 0; k < 4; k++) {
                        out.push_back(reinterpret_cast<char*>(&be)[k]);
                    }
                }
                t.rptr = r.seq.rptr;
                return true;
            }

            template <class In, class Out>
            constexpr bool make_sha1(In&& in, Out& out) {
                auto seq = make_ref_seq(in);
                return make_sha1(seq, out);
            }
        }  // namespace sha

    }  // namespace net
}  // namespace utils
