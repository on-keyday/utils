/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ipaddr - ip address encode/decode
#pragma once
#include <core/byte.h>
#include <cstdint>
#include <number/parse.h>
#include <strutil/append.h>

namespace futils {
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

        constexpr auto bit4_writer(auto& out) {
            return [&](byte d) {
                d = d & 0xf;
                if (d < 0xa) {
                    out.push_back(d + '0');
                }
                else {
                    out.push_back(d - 0xa + 'a');
                }
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
            auto write_4bit = bit4_writer(out);
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
        template <class T>
        constexpr bool parse_ipv4(Sequencer<T>& seq, auto&& out, bool eof = true) {
            static_assert(sizeof(out[1]) == 1);
            auto read = [&](auto& parsed) {
                std::uint8_t b = 0;
                auto conf = number::NumConfig{};
                conf.allow_zero_prefixed = false;
                if (!number::parse_integer(seq, b, 10, conf)) {
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
            std::uint16_t before[10], after[10];
            std::uint8_t ipv4mapped[4];
            bool v4map = false;
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
            size_t omit = 8 - before_count - after_count;
            for (size_t i = 0; i < before_count; i++) {
                out[i * 2] = std::uint8_t((before[i] >> 8) & 0xff);
                out[i * 2 + 1] = std::uint8_t((before[i] >> 0) & 0xff);
            }
            for (size_t i = 0; i < omit; i++) {
                auto idx = (before_count + i);
                out[idx * 2] = 0;
                out[idx * 2 + 1] = 0;
            }
            for (size_t i = 0; i < after_count; i++) {
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

        auto port_writer(auto& out) {
            return [&](std::uint16_t v) {
                auto b1 = v / 10000;
                v -= b1 * 10000;
                auto b2 = v / 1000;
                v -= b2 * 1000;
                auto b3 = v / 100;
                v -= b3 * 100;
                auto b4 = v / 10;
                v -= b4 * 10;
                auto b5 = v;
                if (b1) {
                    out.push_back(b1 + '0');
                }
                if (b1 || b2) {
                    out.push_back(b2 + '0');
                }
                if (b1 || b2 || b3) {
                    out.push_back(b3 + '0');
                }
                if (b1 || b2 || b3 || b4) {
                    out.push_back(b4 + '0');
                }
                out.push_back(b5 + '0');
            };
        }

        constexpr void ipv4_to_string_with_port(auto& out, auto&& addr, std::uint16_t port) {
            ipv4_to_string(out, addr);
            out.push_back(':');
            port_writer(out)(port);
        }

        constexpr void ipv6_to_string_with_port(auto& out, auto&& addr, std::uint16_t port, bool ipv4mapped = false, bool omit_0 = true, bool omit_hex = true) {
            out.push_back('[');
            ipv6_to_string(out, addr, ipv4mapped, omit_0, omit_hex);
            out.push_back(']');
            out.push_back(':');
            port_writer(out)(port);
        }

        template <class T>
        constexpr bool parse_ipv4_with_port(Sequencer<T>& seq, auto&& addr, auto& port, bool eof = true) {
            if (!parse_ipv4(seq, addr, false)) {
                return false;
            }
            if (seq.seek_if(":")) {
                std::uint16_t num;
                if (!number::parse_integer(seq, num, 10)) {
                    return false;
                }
                port = num;
            }
            if (eof) {
                if (!seq.eos()) {
                    return false;
                }
            }
            return true;
        }

        template <class T>
        constexpr bool parse_ipv6_with_port(Sequencer<T>& seq, auto&& addr, auto& port, bool eof = true, bool enable_ipv4mapped = true) {
            if (!seq.seek_if("[")) {
                return false;
            }
            if (!parse_ipv6(seq, addr, false, enable_ipv4mapped)) {
                return false;
            }
            if (!seq.seek_if("]")) {
                return false;
            }
            if (seq.seek_if(":")) {
                std::uint16_t num;
                if (!number::parse_integer(seq, num, 10)) {
                    return false;
                }
                port = num;
            }
            if (eof) {
                if (!seq.eos()) {
                    return false;
                }
            }
            return true;
        }

        template <size_t s>
        struct ByteBuf {
            std::uint8_t addr[s]{};
            std::uint16_t port = 0;
        };

        constexpr std::pair<ByteBuf<4>, bool> to_ipv4(auto&& addr) {
            auto seq = make_ref_seq(addr);
            ByteBuf<4> buf;
            if (!parse_ipv4(seq, buf.addr)) {
                return {buf, false};
            }
            buf.port = 0;
            return {buf, true};
        }

        constexpr std::pair<ByteBuf<16>, bool> to_ipv6(auto&& addr, bool ipv4map = false) {
            auto seq = make_ref_seq(addr);
            ByteBuf<16> buf;
            if (!parse_ipv6(seq, buf.addr, true, ipv4map)) {
                return {buf, false};
            }
            buf.port = 0;
            return {buf, true};
        }

        constexpr std::pair<ByteBuf<4>, bool> to_ipv4withport(auto&& addr) {
            auto seq = make_ref_seq(addr);
            ByteBuf<4> buf;
            int port = -1;
            if (!parse_ipv4_with_port(seq, buf.addr, port)) {
                return {buf, false};
            }
            if (port == -1) {
                buf.port = 0;
            }
            else {
                buf.port = port;
            }
            return {buf, true};
        }

        constexpr std::pair<ByteBuf<16>, bool> to_ipv6withport(auto&& addr, bool ipv4map = false) {
            auto seq = make_ref_seq(addr);
            ByteBuf<16> buf;
            int port = -1;
            if (!parse_ipv6_with_port(seq, buf.addr, port, true, ipv4map)) {
                return {buf, false};
            }
            if (port == -1) {
                buf.port = 0;
            }
            else {
                buf.port = port;
            }
            return {buf, true};
        }

        template <class T>
        constexpr bool parse_dns_ptr_ipv4(Sequencer<T>& seq, auto&& out, bool eof = true) {
            byte data[4];
            if (!parse_ipv4(seq, data, false)) {
                return false;
            }
            if (!seq.seek_if(".in-addr.arpa")) {
                return false;
            }
            if (eof && !seq.eos()) {
                return false;
            }
            out[0] = data[3];
            out[1] = data[2];
            out[2] = data[1];
            out[3] = data[0];
            return true;
        }

        constexpr void ipv4_to_dns_ptr(auto&& out, auto&& addr) {
            static_assert(sizeof(addr[1]) == 1);
            byte data[4];
            data[0] = addr[3];
            data[1] = addr[2];
            data[2] = addr[1];
            data[3] = addr[0];
            ipv4_to_string(out, data);
            strutil::append(out, ".in-addr.arpa");
        }

        template <class T>
        constexpr bool parse_dns_ptr_ipv6(Sequencer<T>& seq, auto&& out, bool eof = false) {
            constexpr auto to_num = [](byte d) {
                if ('0' <= d && d <= '9') {
                    return d - '0';
                }
                else if ('a' <= d && d <= 'f') {
                    return d - 'a' + 0xa;
                }
                else {  // large letter
                    return d - 'A' + 0xA;
                }
            };
            for (size_t i = 0; i < 16; i++) {
                if (i != 0) {
                    if (!seq.consume_if('.')) {
                        return false;
                    }
                }
                auto c = seq.current();
                if (!number::is_hex(c)) {
                    return false;
                }
                byte first = to_num(c);
                seq.consume();
                if (!seq.consume_if('.')) {
                    return false;
                }
                c = seq.current();
                if (!number::is_hex(c)) {
                    return false;
                }
                byte second = to_num(c);
                // 2f -> f.2
                out[15 - i] = byte((second << 4) | first);
            }
            if (!seq.seek_if(".ip6.arpa")) {
                return false;
            }
            if (eof && !seq.eos()) {
                return false;
            }
            return true;
        }

        constexpr void ipv6_to_dns_ptr(auto&& out, auto&& addr) {
            static_assert(sizeof(addr[1]) == 1);
            auto write_4bit = bit4_writer(out);
            for (size_t i = 0; i < 16; i++) {
                if (i != 0) {
                    out.push_back('.');
                }
                byte d = addr[15 - i];
                write_4bit(d & 0xf);
                out.push_back('.');
                write_4bit((d >> 4) & 0xf);
            }
            strutil::append(out, ".ip6.arpa");
        }

    }  // namespace ipaddr
}  // namespace futils
