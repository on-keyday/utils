/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// utf16 - utf16
#pragma once
#include "../../core/byte.h"
#include <cstdint>
#include <utility>
#include <tuple>
#include "../../endian/buf.h"
#include "enabler.h"

namespace utils::utf {

    constexpr size_t expect_utf16_len(std::uint32_t code) noexcept {
        if (code <= 0xFFFF) {
            return 1;
        }
        else if (code <= 0x10FFFF) {
            return 2;
        }
        return 0;
    }

    constexpr bool is_utf16_high_surrogate(std::uint16_t c) noexcept {
        return 0xD800 <= c && c < 0xDC00;
    }

    constexpr bool is_utf16_low_surrogate(std::uint16_t c) noexcept {
        return 0xDC00 <= c && c < 0xE000;
    }

    constexpr std::uint32_t make_surrogate_char(std::uint16_t first, std::uint16_t second) noexcept {
        return 0x10000 + (first - 0xD800) * 0x400 + (second - 0xDC00);
    }

    constexpr std::pair<std::uint16_t, std::uint16_t> split_surrogate_char(std::uint32_t code) noexcept {
        // code = 0x10000 + (first-0xD800) * 0x400 + (second-0xDC00)
        return {
            (code - 0x10000) / 0x400 + 0xD800,
            (code - 0x10000) % 0x400 + 0xDC00,
        };
    }

    constexpr std::uint16_t utf16_bom = 0xFEFF;
    constexpr std::uint16_t utf16_repchar = 0xFFFD;

    template <UTFSize<2> C>
    constexpr std::uint32_t compose_utf32_from_utf16(const C* ptr, size_t len) noexcept {
        switch (len) {
            case 1:
                return ptr[0];
            case 2:
                return make_surrogate_char(ptr[0], ptr[1]);
            default:
                return ~std::uint32_t(0);
        }
    }

    template <UTFSize<1> C>
    constexpr std::uint32_t compose_utf32_from_utf16(const C* ptr, size_t len, bool be) noexcept {
        auto data = [&](const byte* p) {
            if (be) {
                return endian::Buf<std::uint16_t, const C*>{p}.read_be();
            }
            else {
                return endian::Buf<std::uint16_t, const C*>{p}.read_le();
            }
        };
        switch (len) {
            case 1:
                return data(ptr);
            case 2:
                return make_surrogate_char(data(ptr), data(ptr + 2));
            default:
                return ~std::uint32_t(0);
        }
    }

    template <UTFSize<2> C>
    constexpr bool compose_utf16_from_utf32(std::uint32_t code, C* ptr, size_t len) noexcept {
        switch (len) {
            case 1:
                ptr[0] = std::uint16_t(code);
                return true;
            case 2:
                std::tie(ptr[0], ptr[1]) = split_surrogate_char(code);
                return true;
            default:
                return false;
        }
    }

    template <UTFSize<1> C>
    constexpr bool compose_utf16_from_utf32(std::uint32_t code, C* ptr, size_t len, bool be) noexcept {
        auto data = [&](byte* p, std::uint16_t data) {
            if (be) {
                return endian::Buf<std::uint16_t, C*>{p}.write_be(data);
            }
            else {
                return endian::Buf<std::uint16_t, C*>{p}.write_le(data);
            }
        };
        switch (len) {
            case 1:
                data(ptr, std::uint16_t(code));
                return true;
            case 2: {
                auto d = split_surrogate_char(code);
                data(ptr, d.first);
                data(ptr + 2, d.second);
                return true;
            }
            default:
                return false;
        }
    }

    template <UTFSize<2> C>
    constexpr size_t is_valid_utf16(const C* data, size_t size) noexcept {
        if (!data) {
            return 0;
        }
        switch (size) {
            case 0:
                return 0;
            case 1:
                if (is_utf16_high_surrogate(data[0]) ||
                    is_utf16_low_surrogate(data[0])) {
                    return 0;
                }
                return 1;
            default:
                if (is_utf16_high_surrogate(data[0])) {
                    if (!is_utf16_low_surrogate(data[1])) {
                        return 0;
                    }
                    return 2;
                }
                else if (is_utf16_low_surrogate(data[0])) {
                    return 0;
                }
                return 1;
        }
    }

    template <UTFSize<1> C>
    constexpr std::pair<size_t, std::uint16_t> is_valid_utf16(const C* data, size_t size, bool be) noexcept {
        if (!data) {
            return {0, 0};
        }
        auto compose = [&](const C* p) {
            if (be) {
                return endian::Buf<std::uint16_t, const C*>{p}.read_be();
            }
            else {
                return endian::Buf<std::uint16_t, const C*>{p}.read_le();
            }
        };
        switch (size) {
            case 0:
            case 1:
                return {0, 0};
            case 2:
            case 3: {
                auto v = compose(data);
                if (is_utf16_high_surrogate(v) ||
                    is_utf16_low_surrogate(v)) {
                    return {0, v};
                }
                return {1, v};
            }
            default: {
                auto v = compose(data);
                if (is_utf16_high_surrogate(v)) {
                    if (!is_utf16_low_surrogate(compose(data + 2))) {
                        return {0, v};
                    }
                    return {2, v};
                }
                else if (is_utf16_low_surrogate(v)) {
                    return {0, v};
                }
                return {1, v};
            }
        }
    }

    namespace test {
        constexpr bool check_utf16() {
            std::uint16_t data[2];
            auto by = 0;
            auto do_test = [&](std::uint32_t code, size_t len) {
                if (expect_utf16_len(code) != len) {
                    by = 1;
                    return false;
                }
                if (!compose_utf16_from_utf32(code, data, len)) {
                    by = 2;
                    return false;
                }
                if (len == 2) {
                    if (!is_utf16_high_surrogate(data[0]) ||
                        !is_utf16_low_surrogate(data[1])) {
                        by = 3;
                        return false;
                    }
                }
                else {
                    if (is_utf16_high_surrogate(data[0]) || is_utf16_low_surrogate(data[0])) {
                        by = 3;
                        return false;
                    }
                }
                if (compose_utf32_from_utf16(data, len) != code) {
                    by = 4;
                    return false;
                }
                return (bool)is_valid_utf16(data, len);
            };
            bool ok = do_test(U'🎅', 2) &&
                      do_test('h', 1) &&
                      !do_test(0xDC00, 1) &&
                      by == 3 &&
                      !do_test(0x110000, 2) &&
                      by == 1;
            if (!ok) {
                return false;
            }
            byte data2[4];
            auto code = U'🎅';
            if (!compose_utf16_from_utf32(code, data2, 2, true)) {
                return false;
            }
            if (compose_utf32_from_utf16(data2, 2, true) != code) {
                return false;
            }
            return is_valid_utf16(data2, 4, true).first;
        }

        static_assert(check_utf16(), "ok");
    }  // namespace test

}  // namespace utils::utf
