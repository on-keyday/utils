/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// varint - QUIC variable integer
#pragma once
#include "../../binary/buf.h"
#include "../../view/iovec.h"
#include "../../binary/writer.h"
#include "../../binary/reader.h"
#include <tuple>

namespace futils {
    namespace fnet::quic::varint {
        struct Value {
            std::uint64_t value = 0;
            byte len = 0;

            constexpr Value() = default;
            constexpr Value(std::uint64_t v)
                : value(v), len(0) {}
            constexpr Value(std::uint64_t v, byte l)
                : value(v), len(l) {}

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

        constexpr bool is_min_len(const Value& value) {
            return is_min_len(value.value, value.len);
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
            if (value.len == 0) {
                return min_len(value.value);
            }
            return value.len;
        }

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

        constexpr std::tuple<Value, bool> read(binary::reader& input) {
            if (input.empty()) {
                return {Value{}, false};
            }
            const auto vlen = mask_to_len(input.remain()[0]);
            auto [data, ok] = input.read(vlen);
            if (!ok) {
                return {Value(0, vlen), false};
            }
            Value value;
            value.len = vlen;
            value.value = read_unchecked(data, vlen);
            return {value, true};
        }

        constexpr bool read(binary::reader& input, Value& output) {
            bool ok;
            std::tie(output, ok) = read(input);
            return ok;
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

        constexpr bool write(binary::writer& output, const Value& value) {
            auto rem = output.remain();
            if (value.len == 0) {
                auto len = min_len(value.value);
                if (len == 0) {
                    return false;
                }
                if (rem.size() < len) {
                    return false;
                }
                write_unchecked(rem, value.value, len);
                output.offset(len);
            }
            else {
                if (!in_range(value.value, value.len)) {
                    return false;
                }
                if (rem.size() < value.len) {
                    return false;
                }
                write_unchecked(rem, value.value, value.len);
                output.offset(value.len);
            }
            return true;
        }

    }  // namespace fnet::quic::varint
}  // namespace futils
