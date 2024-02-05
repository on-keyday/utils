/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../binary/number.h"
#include <fnet/error.h>
#include <binary/log2i.h>

namespace futils::fnet::quic::packet {

    struct WireVal {
        std::uint32_t value = 0;
        byte len = 0;
    };

    struct PacketNumber {
        std::uint64_t value = 0;

        constexpr PacketNumber() = default;

        constexpr PacketNumber(std::uint64_t v) noexcept
            : value(v) {}

        constexpr operator std::uint64_t() const noexcept {
            return value;
        }
    };

    constexpr PacketNumber infinity = {~std::uint64_t(0)};

    constexpr bool is_wire_len(byte len) noexcept {
        return 1 <= len && len <= 4;
    }

    constexpr expected<PacketNumber> decode(std::uint32_t value, byte len, std::uint64_t largest_pn) noexcept {
        if (!is_wire_len(len)) {
            return unexpect("unexpected wire len");
        }
        auto expected_pn = largest_pn + 1;
        auto pn_win = 1 << (len << 3);
        auto pn_mask = pn_win - 1;
        auto base = (std::uint64_t(expected_pn) & ~pn_mask);
        std::uint64_t next = base + pn_win;
        std::uint64_t prev = base < pn_win ? 0 : base - pn_win;
        auto delta = [](std::uint64_t a, std::uint64_t b) {
            if (a < b) {
                return b - a;
            }
            return a - b;
        };
        auto closer = [&](std::uint64_t t, std::uint64_t a, std::uint64_t b) {
            if (delta(t, a) < delta(t, b)) {
                return a;
            }
            return b;
        };
        auto selected = closer(expected_pn, base + value, closer(expected_pn, prev + value, next + value));
        return selected;
    }

    constexpr expected<PacketNumber> decode(WireVal pn_wire, std::uint64_t largest_pn) noexcept {
        return decode(pn_wire.value, pn_wire.len, largest_pn);
    }

    /*
    namespace internal {

        constexpr std::uint64_t log2i(std::uint64_t bit) noexcept {
            if (bit == 1) {
                return 0;
            }
            if (bit == 2 || bit == 3) {
                return 1;
            }
            for (auto i = 0; i < 64; i++) {
                if (bit & (std::uint64_t(1) << (63 - i))) {
                    return 63 - i;
                }
            }
            return ~0;
        }
    }  // namespace internal
    */

    constexpr expected<WireVal> encode(std::uint64_t pn, std::uint64_t largest_ack) noexcept {
        size_t num_unacked = 0;
        if (largest_ack == ~0) {
            num_unacked = pn + 1;
        }
        else {
            num_unacked = pn - size_t(largest_ack);
        }
        auto min_bits = binary::log2i(num_unacked) + 1;
        auto min_bytes = min_bits / 8 + (min_bits % 8 ? 1 : 0);
        auto wire = [&](std::uint32_t mask, byte len) {
            return WireVal{std::uint32_t(pn & mask), len};
        };
        if (min_bytes == 1) {
            return wire(0xff, 1);
        }
        if (min_bytes == 2) {
            return wire(0xffff, 2);
        }
        if (min_bytes == 3) {
            return wire(0xffffff, 3);
        }
        if (min_bytes == 4) {
            return wire(0xffffffff, 4);
        }
        return unexpect("expect value must be in range 1-4");
    }

    constexpr expected<void> write(binary::writer& w, const WireVal& value) noexcept {
        if (!is_wire_len(value.len)) {
            return unexpect("unexpected wire len");
        }
        binary::Buf<std::uint32_t> data;
        data.write_be(value.value);
        if (!w.write(view::rvec(data.data + 4 - value.len, value.len))) {
            return unexpect("not enough space");
        }
        return {};
    }

    constexpr expected<void> read(binary::reader& r, std::uint32_t& value, byte len) noexcept {
        if (!is_wire_len(len)) {
            return unexpect("unexpected wire len");
        }
        auto [buf, ok] = r.read(len);
        if (!ok) {
            return unexpect("not enough space");
        }
        binary::Buf<std::uint32_t> data{};
        for (size_t i = 0; i < len; i++) {
            data[4 - len + i] = buf[i];
        }
        data.read_be(value);
        return {};
    }

    constexpr expected<WireVal> read(binary::reader& r, byte len) noexcept {
        std::uint32_t value;
        if (auto res = read(r, value, len); !res) {
            return unexpect(res.error());
        }
        return WireVal{.value = value, .len = len};
    }

    namespace test {

        constexpr bool check_packet_number_encoding() {
            constexpr auto expect = 0x9394939393;
            auto wire = encode(expect, 0x9394933293);
            if (!wire) {
                return false;
            }
            byte data[100];
            binary::writer w{data};
            if (!write(w, *wire)) {
                return false;
            }
            binary::reader r{w.written()};
            std::uint32_t value;
            if (!read(r, value, wire->len)) {
                return false;
            }
            // on flight, packet order is changed ....
            auto decoded = decode(value, wire->len, 0x9394933301);
            if (!decoded) {
                return false;
            }
            return decoded->value == expect;
        }

        static_assert(check_packet_number_encoding());
    }  // namespace test

}  // namespace futils::fnet::quic::packet
