/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// utf8 - utf convertion
#pragma once
#include "../../core/byte.h"
#include <cstdint>
#include "enabler.h"
#include "../../endian/buf.h"
#include <tuple>

namespace utils::utf {
    constexpr size_t expect_utf8_len(std::uint32_t c) noexcept {
        if (c < 0x80) {
            return 1;
        }
        else if (c < 0x800) {
            return 2;
        }
        else if (c < 0x10000) {
            return 3;
        }
        else if (c < 0x110000) {
            return 4;
        }
        return 0;
    }

    constexpr size_t utf8_first_byte_to_len(byte data) noexcept {
        if ((data & 0x80) == 0) {
            return 1;
        }
        else if ((data & 0xE0) == 0xC0) {
            return 2;
        }
        else if ((data & 0xF0) == 0xE0) {
            return 3;
        }
        else if ((data & 0xF8) == 0xF0) {
            return 4;
        }
        return 0;
    }

    constexpr byte utf8_first_byte_mask[5]{
        0x80,  // 0xxx-xxxx  1000-0000
        0xC0,  // 10xx-xxxx  1100-0000
        0xE0,  // 110y-yyyx  1110-0000
        0xF0,  // 1110-yyyy  1111-0000
        0xF8,  // 1111-0yyy  1111-1000
    };

    constexpr bool is_utf8_second_later_byte(byte b) noexcept {
        return 0x80 <= b && b <= 0xBF;
    }

    constexpr bool is_utf8_valid_range(size_t len, byte one, byte two) noexcept {
        auto normal_two_range = [&] {
            return is_utf8_second_later_byte(two);
        };
        switch (len) {
            case 1:
                return one <= 0x7f;
            case 2:
                return 0xC2 <= one && one <= 0xDF &&
                       normal_two_range();
            case 3:
                switch (one) {
                    case 0xE0:
                        return 0xA0 <= two && two <= 0xBF;
                    case 0xED:
                        return 0x80 <= two && two <= 0x9F;
                    default:
                        return 0xE0 <= one && one <= 0xEF &&
                               normal_two_range();
                }
            case 4:
                switch (one) {
                    case 0xF0:
                        return 0x90 <= two && two <= 0xBF;
                    case 0xF4:
                        return 0x80 <= two && two <= 0x8F;
                    default:
                        return 0xF0 <= one && one <= 0xF4 &&
                               normal_two_range();
                }
            default:
                return false;
        }
    }

    constexpr byte utf8_bnot_first_byte[]{
        byte(~utf8_first_byte_mask[0]),
        byte(~utf8_first_byte_mask[1]),
        byte(~utf8_first_byte_mask[2]),
        byte(~utf8_first_byte_mask[3]),
        byte(~utf8_first_byte_mask[4]),
    };

    template <UTFSize<1> C>
    constexpr std::uint32_t compose_utf32_from_utf8(const C* data, size_t len) noexcept {
        constexpr auto& b = utf8_bnot_first_byte;
        switch (len) {
            case 1:
                return byte(data[0]) & b[0];
            case 2:
                return std::uint32_t(byte(data[0]) & b[2]) << 6 |
                       std::uint32_t(byte(data[1]) & b[1]);
            case 3:
                return std::uint32_t(byte(data[0]) & b[3]) << 12 |
                       std::uint32_t(byte(data[1]) & b[1]) << 6 |
                       std::uint32_t(byte(data[2]) & b[1]);
            case 4:
                return std::uint32_t(byte(data[0]) & b[4]) << 18 |
                       std::uint32_t(byte(data[1]) & b[1]) << 12 |
                       std::uint32_t(byte(data[2]) & b[1]) << 6 |
                       std::uint32_t(byte(data[3]) & b[1]);
            default:
                return ~std::uint32_t(0);
        }
    }

    template <UTFSize<1> C>
    constexpr bool compose_utf8_from_utf32(std::uint32_t code, C* data, size_t len) noexcept {
        constexpr auto& b = utf8_bnot_first_byte;
        constexpr auto& n = utf8_first_byte_mask;
        switch (len) {
            case 1:
                data[0] = byte(code);
                return true;
            case 2:
                data[0] = byte((code >> 6) & b[2]) | n[1];
                data[1] = byte(code & b[1]) | n[0];
                return true;
            case 3:
                data[0] = byte((code >> 12) & b[3]) | n[2];
                data[1] = byte((code >> 6) & b[1]) | n[0];
                data[2] = byte(code & b[1]) | n[0];
                return true;
            case 4:
                data[0] = byte((code >> 18) & b[4]) | n[3];
                data[1] = byte((code >> 12) & b[1]) | n[0];
                data[2] = byte((code >> 6) & b[1]) | n[0];
                data[3] = byte(code & b[1]) | n[0];
                return true;
            default:
                return false;
        }
    }

    constexpr byte utf8_bom[] = {0xEF, 0xBB, 0xBF};

    constexpr byte utf8_repchar[] = {0xEF, 0xBF, 0xBD};

    template <UTFSize<1> C>
    constexpr size_t is_valid_utf8(const C* data, size_t size) noexcept {
        if (!data) {
            return 0;
        }
        size_t len = 0;
        auto check_1 = [&] {
            if (byte(data[0]) <= 0x7F) {
                len = 1;
                return true;
            }
            return false;
        };
        auto check_2 = [&] {
            if (is_utf8_valid_range(2, data[0], data[1])) {
                len = 2;
                return true;
            }
            return false;
        };
        auto check_3 = [&] {
            if (is_utf8_valid_range(3, data[0], data[1]) &&
                is_utf8_second_later_byte(data[2])) {
                len = 3;
                return true;
            }
            return false;
        };
        auto check_4 = [&] {
            if (is_utf8_valid_range(4, data[0], data[1]) &&
                is_utf8_second_later_byte(data[2]) &&
                is_utf8_second_later_byte(data[3])) {
                len = 4;
                return true;
            }
            return false;
        };
        switch (size) {
            case 0:
                return 0;
            case 1:
                (void)check_1();
                break;
            case 2:
                (void)(check_1() || check_2());
                break;
            case 3:
                (void)(check_1() || check_2() || check_3());
                break;
            default:
                (void)(check_1() || check_2() || check_3() || check_4());
        }
        return len;
    }

    namespace test {
        constexpr bool check_utf8() {
            byte data[4]{};
            byte by = 0;
            auto fill = [&](byte a, byte b, byte c, byte d) {
                data[0] = a;
                data[1] = b;
                data[2] = c;
                data[3] = d;
                return true;
            };

            auto do_enc = [&](std::uint32_t code, std::uint32_t len) {
                if (expect_utf8_len(code) != len) {
                    by = 1;
                    return false;
                }
                if (!compose_utf8_from_utf32(code, data, len)) {
                    by = 2;
                    return false;
                }
                return true;
            };
            auto do_dec = [&](std::uint32_t code, std::uint32_t len) {
                if (!is_utf8_valid_range(len, data[0], data[1])) {
                    by = 3;
                    return false;
                }
                if (compose_utf32_from_utf8(data, len) != code) {
                    by = 4;
                    return false;
                }
                return true;
            };
            auto do_test = [&](std::uint32_t code, std::uint32_t len) {
                return do_enc(code, len) && do_dec(code, len);
            };
            bool ok = do_test(U'🎅', 4) &&
                      do_test(U'吉', 3) &&
                      do_test(U'Ω', 2) &&
                      do_test(U'a', 1) &&
                      !do_enc(0x110000, 4) &&
                      by == 1;
            if (!ok) {
                return false;
            };
            ok = fill(0xC0, 0x80, 0, 0) &&
                 !do_dec(0, 2) &&
                 by == 3 &&
                 fill(0xE0, 0x90, 0x80, 0) &&
                 !do_dec(0, 3) &&
                 by == 3;
            fill(0xF0, 0x9f, 0x90, 0xb6);
            return ok && is_valid_utf8(data, 4);
        }

        static_assert(check_utf8(), "convertion not working");
    }  // namespace test
}  // namespace utils::utf
