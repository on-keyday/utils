/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ipaddr - ip address encode/decode
#pragma once
#include <cstdint>
#include <number/parse.h>

namespace utils {
    namespace ipaddr {

        constexpr auto ipv4_writer(auto& out) {
            return [&](std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
                auto write_digit = [&](auto b) {
                    out.push_back(b + '0');
                };
                auto write_byte = [&](auto b) {
                    auto d1 = b / 100;
                    auto d2 = (b - d1 * 100) / 10;
                    auto d3 = (b - d1 * 100 - d2 * 10);
                    if (d1) {
                        write_digit(d1);
                    }
                    if (d1 || d2) {
                        write_digit(d2);
                    }
                    write_digit(d3);
                };
                write_byte(a);
                out.push_back('.');
                write_byte(b);
                out.push_back('.');
                write_byte(c);
                out.push_back('.');
                write_byte(d);
            };
        }

        // ipv4_to_string make ipv4 stringify
        // ipv4 is byte array of size 4 that is accessible with index
        constexpr void ipv4_to_string(auto& out, auto&& ipv4) {
            static_assert(sizeof(ipv4[1]) == 1);
            auto writev4 = ipv4_writer(out);
            writev4(ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
        }

        // ipv6_to_string make ipv6 stringify
        // ipv6 is byte array of size 16 that is accessible with index
        constexpr void ipv6_to_string(auto& out, auto&& ipv6, bool ipv4mapped = false, bool omit_0 = true, bool omit_hex = true) {
            static_assert(sizeof(ipv6[1]) == 1);
            std::uint16_t data[8];
            int omit_begin = -1, omit_count = -1;
            for (auto i = 0; i < 8; i++) {
                data[i] = (std::uint16_t(ipv6[i * 2]) << 8) | ipv6[i * 2 + 1];
            }
            if (omit_0) {
                for (auto i = 0; i < 8; i++) {
                    if (data[i] == 0) {
                        int count = 1;
                        for (auto k = i + 1; k < 8; k++) {
                            if (data[k]) {
                                break;
                            }
                            count++;
                        }
                        if (count >= 2 && count > omit_count) {
                            omit_begin = i;
                            omit_count = count;
                            i += count - 1;
                        }
                    }
                }
            }
            auto write_4bit = [&](auto b) {
                std::uint8_t d = b & 0xf;
                if (d < 0xa) {
                    out.push_back(d + '0');
                }
                else {
                    out.push_back(d - 0xa + 'a');
                }
            };
            auto write_16bit = [&](std::uint16_t b) {
                auto b1 = (b >> 12) & 0xf;
                auto b2 = (b >> 8) & 0xf;
                auto b3 = (b >> 4) & 0xf;
                auto b4 = (b >> 0) & 0xf;
                if (!omit_hex) {
                    write_4bit(b1);
                    write_4bit(b2);
                    write_4bit(b3);
                    write_4bit(b4);
                    return;
                }
                if (b1) {
                    write_4bit(b1);
                }
                if (b1 || b2) {
                    write_4bit(b2);
                }
                if (b1 || b2 || b3) {
                    write_4bit(b3);
                }
                write_4bit(b4);
            };
            int range = ipv4mapped ? 6 : 8;
            for (auto i = 0; i < range; i++) {
                if (i == omit_begin) {
                    if (i == 0) {
                        out.push_back(':');
                    }
                    out.push_back(':');
                    i += omit_count - 1;
                    continue;
                }
                write_16bit(data[i]);
                if (i != 7) {
                    out.push_back(':');
                }
            }
            if (ipv4mapped) {
                auto writev4 = ipv4_writer(out);
                writev4((data[6] >> 8) & 0xff, data[6] & 0xff, (data[7] >> 8) & 0xff, data[7] & 0xff);
            }
        }

        // parse_ipv4 parses text format ipv4 address
        // because of internal parser implementation
        // zero prefix may be allowed like below:
        // 000127.0000.000.01
        template <class T>
        constexpr bool parse_ipv4(Sequencer<T>& seq, auto&& out, bool eof = true) {
            static_assert(sizeof(out[1]) == 1);
            auto read = [&](auto& parsed) {
                std::uint8_t b = 0;
                if (!number::parse_integer(seq, b, 10)) {
                    return false;
                }
                parsed = b;
                return true;
            };
            if (!read(out[0]) ||
                !seq.consume_if('.') ||
                !read(out[1]) ||
                !seq.consume_if('.') ||
                !read(out[2]) ||
                !seq.consume_if('.') ||
                !read(out[3])) {
                return false;
            }
            if (eof && !seq.eos()) {
                return false;
            }
            return true;
        }

        // parse_ipv6 parses ipv6 text format
        // this function can parse ipv4-mapped address
        // see also description of parse_ipv4
        // if enable_v4mapped is false, ipv4-mapped address will not be parsed
        template <class T>
        constexpr bool parse_ipv6(Sequencer<T>& seq, auto&& out, bool eof = true, bool enable_v4mapped = true) {
            static_assert(sizeof(out[1]) == 1);
            size_t max_parsed = 0;
            std::uint16_t before[10], after[10];
            std::uint8_t ipv4mapped[4];
            bool v4map = false;
            int v4mapindex = -1;
            auto read = [&](auto& parsed) {
                return number::parse_integer(seq, parsed, 16);
            };
            auto convert_ipv4 = [&] {
                return std::pair<std::uint16_t, std::uint16_t>{
                    std::uint16_t((std::uint16_t(ipv4mapped[0]) << 8) | ipv4mapped[1]),
                    std::uint16_t((std::uint16_t(ipv4mapped[2]) << 8) | ipv4mapped[3]),
                };
            };
            auto read_ipv4 = [&] {
                if (!enable_v4mapped) {
                    return false;
                }
                auto base = seq.rptr;
                if (!parse_ipv4(seq, ipv4mapped, false)) {
                    seq.rptr = base;
                    return false;
                }
                v4map = true;
                return true;
            };
            size_t before_count = 0, after_count = 0;
            size_t prevpos = seq.rptr;
            for (before_count = 0; before_count < 8; before_count++) {
                if (seq.seek_if("::")) {
                    break;
                }
                if (before_count != 0) {
                    if (!seq.seek_if(":")) {
                        seq.rptr = prevpos;
                        if (read_ipv4()) {
                            if (before_count <= 1) {
                                return false;
                            }
                            before_count -= 1;
                            v4mapindex = before_count;
                            auto [h, l] = convert_ipv4();
                            before[before_count] = h;
                            before[before_count + 1] = l;
                            before_count += 2;
                            break;
                        }
                        return false;
                    }
                }
                auto base = seq.rptr;
                prevpos = seq.rptr;
                if (!read(before[before_count])) {
                    return false;
                }
            }
            if (!v4map && read_ipv4()) {
                auto [h, l] = convert_ipv4();
                after[before_count] = h;
                after[before_count + 1] = l;
                after_count += 2;
            }
            else {
                after_count = 0;
            }
            prevpos = seq.rptr;
            for (; !v4map && after_count < 8 - before_count; after_count++) {
                if (after_count != 0) {
                    if (!seq.seek_if(":")) {
                        auto cur = seq.rptr;
                        seq.rptr = prevpos;
                        if (read_ipv4()) {
                            if (after_count < 1) {
                                return false;
                            }
                            after_count -= 1;
                            v4mapindex = after_count;
                            auto [h, l] = convert_ipv4();
                            after[after_count] = h;
                            after[after_count + 1] = l;
                            after_count += 2;
                            break;
                        }
                        seq.rptr = cur;
                        break;
                    }
                }
                auto base = seq.rptr;
                prevpos = seq.rptr;
                if (!read(after[after_count])) {
                    if (after_count == 0) {
                        break;
                    }
                    return false;
                }
            }
            if (before_count + after_count > 8) {
                return false;  // too large count
            }
            if (eof && !seq.eos()) {
                return false;  // not eof
            }
            if (before_count == 8) {
                for (auto i = 0; i < 8; i++) {
                    out[i * 2] = std::uint8_t((before[i] >> 8) & 0xff);
                    out[i * 2 + 1] = std::uint8_t((before[i] >> 0) & 0xff);
                }
                return true;
            }
            if (before_count + after_count >= 8) {
                return false;  // least one element should be omited
            }
            if (v4map && after_count == 0) {
                return false;
            }
            auto omit = 8 - before_count - after_count;
            for (auto i = 0; i < before_count; i++) {
                out[i * 2] = std::uint8_t((before[i] >> 8) & 0xff);
                out[i * 2 + 1] = std::uint8_t((before[i] >> 0) & 0xff);
            }
            for (auto i = 0; i < omit; i++) {
                auto idx = (before_count + i);
                out[idx * 2] = 0;
                out[idx * 2 + 1] = 0;
            }
            for (auto i = 0; i < after_count; i++) {
                auto idx = (before_count + omit + i);
                out[idx * 2] = std::uint8_t((after[i] >> 8) & 0xff);
                out[idx * 2 + 1] = std::uint8_t((after[i] >> 0) & 0xff);
            }
            return true;
        }

        // is_ipv4_mapped returns whether ipv6 address is ipv4 mapped address
        // in should be byte array of size 16
        constexpr auto is_ipv4_mapped(auto&& in) {
            return in[2 * 0] == 0x00 &&
                   in[2 * 0 + 1] == 0x00 &&
                   in[2 * 1] == 0x00 &&
                   in[2 * 1 + 1] == 0x00 &&
                   in[2 * 2] == 0x00 &&
                   in[2 * 2 + 1] == 0x00 &&
                   in[2 * 3] == 0x00 &&
                   in[2 * 3 + 1] == 0x00 &&
                   in[2 * 4] == 0x00 &&
                   in[2 * 4 + 1] == 0x00 &&
                   in[2 * 5] == 0xff &&
                   in[2 * 5 + 1] == 0xff;
        }
    }  // namespace ipaddr
}  // namespace utils
