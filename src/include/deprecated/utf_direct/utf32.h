/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// utf32
#pragma once
#include <cstdint>
#include "../../endian/buf.h"
#include <utility>
#include "enabler.h"

namespace utils::utf {
    constexpr size_t expect_utf32_len(std::uint32_t code) noexcept {
        return code <= 0x10FFFF ? 1 : 0;
    }

    constexpr std::uint32_t utf32_bom = 0xFEFF;
    constexpr std::uint32_t utf32_repchar = 0xFFFD;

    constexpr std::uint32_t compose_utf32_from_utf32(const byte* ptr, size_t len, bool be) noexcept {
        auto data = [&](const byte* p) {
            if (be) {
                return endian::Buf<std::uint32_t, const byte*>{p}.read_be();
            }
            else {
                return endian::Buf<std::uint32_t, const byte*>{p}.read_le();
            }
        };
        switch (len) {
            case 1:
                return data(ptr);
            default:
                return ~std::uint32_t(0);
        }
    }

    constexpr bool compose_utf32_from_utf32(std::uint32_t code, byte* ptr, size_t len, bool be) noexcept {
        auto data = [&](byte* p, std::uint32_t data) {
            if (be) {
                return endian::Buf<std::uint32_t, byte*>{p}.write_be(data);
            }
            else {
                return endian::Buf<std::uint32_t, byte*>{p}.write_le(data);
            }
        };
        switch (len) {
            case 1:
                data(ptr, std::uint32_t(code));
                return true;
            default:
                return false;
        }
    }

    template <UTFSize<4> C>
    constexpr size_t is_valid_utf32(const C* data, size_t size) noexcept {
        if (!data) {
            return 0;
        }
        switch (size) {
            case 0:
                return 0;
            default:
                return expect_utf32_len(data[0]);
        }
    }

    template <UTFSize<1> C>
    constexpr std::pair<size_t, std::uint32_t> is_valid_utf32(const C* data, size_t size, bool be) noexcept {
        if (!data) {
            return {0, 0};
        }
        auto compose = [&] {
            if (be) {
                return endian::Buf<std::uint32_t, const C*>{data}.read_be();
            }
            else {
                return endian::Buf<std::uint32_t, const C*>{data}.read_le();
            }
        };
        switch (size) {
            case 0:
            case 1:
            case 2:
            case 3:
                return {0, 0};
            default: {
                const auto v = compose();
                return {expect_utf32_len(v), v};
            }
        }
    }
}  // namespace utils::utf
