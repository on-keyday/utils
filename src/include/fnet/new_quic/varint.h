/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// varint - QUIC variable integer
#pragma once
#include <binary/writer.h>
#include <binary/reader.h>
#include <binary/buf.h>
#include <fnet/error.h>

namespace utils::fnet::quic::varint {

    struct Value {
        std::uint64_t value = 0;

        constexpr Value() = default;
        constexpr Value(std::uint64_t v)
            : value(v) {}

        constexpr operator std::uint64_t() const noexcept {
            return value;
        }
    };

    // exclude
    constexpr std::uint64_t border(byte len) {
        switch (len) {
            case 1:
                return 0x40;
            case 2:
                return 0x4000;
            case 4:
                return 0x40000000;
            case 8:
                return 0x4000000000000000;
        }
        return 0;
    }

    constexpr bool in_range(std::uint64_t value, byte len) {
        return value < border(len);
    }

    constexpr size_t min_len(std::uint64_t value) {
        if (in_range(value, 1)) {
            return 1;
        }
        if (in_range(value, 2)) {
            return 2;
        }
        if (in_range(value, 4)) {
            return 4;
        }
        if (in_range(value, 8)) {
            return 8;
        }
        return 0;
    }

    constexpr bool is_min_len(std::uint64_t value, byte len) {
        return min_len(value) == len;
    }

    constexpr byte len_mask(byte len) {
        switch (len) {
            case 1:
                return 0x00;
            case 2:
                return 0x40;
            case 4:
                return 0x80;
            case 8:
                return 0xC0;
        }
        return 0xff;
    }

    constexpr std::uint64_t len_bit_mask(byte bwide) {
        switch (bwide) {
            case 1:
                return 0xC0;
            case 2:
                return 0xC000;
            case 4:
                return 0xC0000000;
            case 8:
                return 0xC000000000000000;
            default:
                return 0;
        }
    }

    constexpr byte mask_to_len(byte mask) {
        switch (mask & 0xC0) {
            case 0x00:
                return 1;
            case 0x40:
                return 2;
            case 0x80:
                return 4;
            case 0xC0:
                return 8;
            default:
                return 0;
        }
    }

    constexpr byte len(const Value& value) {
        return min_len(value.value);
    }

    namespace internal {
        constexpr std::uint64_t read_unchecked(auto&& input, byte len) {
            std::uint64_t data;
            switch (len) {
                case 1:
                    data = input[0];
                    break;
                case 2:
                    data = binary::Buf<std::uint16_t, decltype(input)>{input}.read_be();
                    break;
                case 4:
                    data = binary::Buf<std::uint32_t, decltype(input)>{input}.read_be();
                    break;
                case 8:
                    data = binary::Buf<std::uint64_t, decltype(input)>{input}.read_be();
                    break;
                default:
                    // undefined behaviour
                    return ~std::uint64_t(0);
            }
            data &= ~len_bit_mask(len);
            return data;
        }

        constexpr void write_unchecked(auto&& output, std::uint64_t value, byte len) {
            switch (len) {
                case 1:
                    output[0] = byte(value);
                    return;
                case 2:
                    binary::Buf<std::uint16_t, decltype(output)>{output}.write_be(value)[0] |= len_mask(2);
                    return;
                case 4:
                    binary::Buf<std::uint32_t, decltype(output)>{output}.write_be(value)[0] |= len_mask(4);
                    return;
                case 8:
                    binary::Buf<std::uint64_t, decltype(output)>{output}.write_be(value)[0] |= len_mask(8);
                    return;
            }
        }
    }  // namespace internal

    constexpr expected<Value> read(binary::reader& input, byte* len = nullptr) {
        if (input.empty()) {
            return unexpect("empty input");
        }
        const auto vlen = mask_to_len(input.remain()[0]);
        auto [data, ok] = input.read(vlen);
        if (!ok) {
            return unexpect("not read data for varint");
        }
        if (len != nullptr) {
            *len = vlen;
        }
        Value value;
        value.value = internal::read_unchecked(data, vlen);
        return value;
    }

    constexpr expected<void> read(binary::reader& input, Value& output) {
        return read(input).transform([&](Value&& value) {
            output = value;
        });
    }

    constexpr expected<void> write(binary::writer& output, const Value& value, byte len = 0) {
        auto rem = output.remain();
        if (len == 0) {
            auto len = min_len(value.value);
            if (len == 0) {
                return unexpect("value out of range");
            }
            if (rem.size() < len) {
                return unexpect("output buffer too small");
            }
            internal::write_unchecked(rem, value.value, len);
            output.offset(len);
        }
        else {
            if (!in_range(value.value, len)) {
                return unexpect("value out of range for len");
            }
            if (rem.size() < len) {
                return unexpect("output buffer too small");
            }
            internal::write_unchecked(rem, value.value, len);
            output.offset(len);
        }
        return {};
    }

}  // namespace utils::fnet::quic::varint
