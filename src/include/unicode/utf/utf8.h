/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// utf8 - utf8
#pragma once
#include "../../core/byte.h"
#include <cstdint>
#include "unicode.h"
#include "output.h"
#include <tuple>
#include "error.h"
#include "../../core/sequencer.h"

namespace utils::unicode::utf8 {

    constexpr byte utf8_first_byte_mask[5]{
        0x80,  // 0xxx-xxxx  1000-0000
        0xC0,  // 10xx-xxxx  1100-0000
        0xE0,  // 110y-yyyx  1110-0000
        0xF0,  // 1110-yyyy  1111-0000
        0xF8,  // 1111-0yyy  1111-1000
    };

    constexpr byte utf8_bnot_first_byte[]{
        byte(~utf8_first_byte_mask[0]),
        byte(~utf8_first_byte_mask[1]),
        byte(~utf8_first_byte_mask[2]),
        byte(~utf8_first_byte_mask[3]),
        byte(~utf8_first_byte_mask[4]),
    };

    constexpr byte utf8_bom[] = {0xEF, 0xBB, 0xBF};

    constexpr byte utf8_repchar[] = {0xEF, 0xBF, 0xBD};

    constexpr size_t expect_len(std::uint32_t c) noexcept {
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

    constexpr bool in_byte_range(auto c) noexcept {
        return 0 <= c && c <= 0xff;
    }

    constexpr size_t first_byte_to_len(byte data) noexcept {
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

    constexpr bool is_second_later_byte(auto b) noexcept {
        if constexpr (sizeof(b) != 1) {
            return 0x80 <= b && b <= 0xBF;
        }
        else {
            return 0x80 <= byte(b) && byte(b) <= 0xBF;
        }
    }

    constexpr bool valid_range(size_t len, auto one, auto two) noexcept {
        if constexpr (sizeof(one) != 1) {
            if (!in_byte_range(one)) {
                return false;
            }
        }
        if constexpr (sizeof(two) != 1) {
            if (!in_byte_range(two)) {
                return false;
            }
        }
        auto do_judge = [&](byte one, byte two) {
            switch (len) {
                case 1:
                    return one <= 0x7f;
                case 2:
                    return 0xC2 <= one && one <= 0xDF &&
                           is_second_later_byte(two);
                case 3:
                    switch (one) {
                        case 0xE0:
                            return 0xA0 <= two && two <= 0xBF;
                        case 0xED:
                            return 0x80 <= two && two <= 0x9F;
                        default:
                            return 0xE0 <= one && one <= 0xEF &&
                                   is_second_later_byte(two);
                    }
                case 4:
                    switch (one) {
                        case 0xF0:
                            return 0x90 <= two && two <= 0xBF;
                        case 0xF4:
                            return 0x80 <= two && two <= 0x8F;
                        default:
                            return 0xF0 <= one && one <= 0xF4 &&
                                   is_second_later_byte(two);
                    }
                default:
                    return false;
            }
        };
        return do_judge(byte(one), byte(two));
    }

    constexpr std::uint32_t decode_one_unchecked(auto&& data, size_t offset, size_t len) noexcept(noexcept(data[1])) {
        constexpr auto& b = utf8_bnot_first_byte;
        switch (len) {
            case 1:
                return byte(data[offset]) & b[0];
            case 2:
                return std::uint32_t(byte(data[offset]) & b[2]) << 6 |
                       std::uint32_t(byte(data[offset + 1]) & b[1]);
            case 3:
                return std::uint32_t(byte(data[offset]) & b[3]) << 12 |
                       std::uint32_t(byte(data[offset + 1]) & b[1]) << 6 |
                       std::uint32_t(byte(data[offset + 2]) & b[1]);
            case 4:
                return std::uint32_t(byte(data[offset]) & b[4]) << 18 |
                       std::uint32_t(byte(data[offset + 1]) & b[1]) << 12 |
                       std::uint32_t(byte(data[offset + 2]) & b[1]) << 6 |
                       std::uint32_t(byte(data[offset + 3]) & b[1]);
            default:
                return invalid_code;
        }
    }

    static_assert(decode_one_unchecked(utf8_bom, 0, 3) == byte_order_mark);
    static_assert(decode_one_unchecked(utf8_repchar, 0, 3) == replacement_character);

    constexpr bool encode_one_unchecked(auto&& output, std::uint32_t code, size_t len) {
        byte data[4]{};
        constexpr auto& b = utf8_bnot_first_byte;
        constexpr auto& n = utf8_first_byte_mask;
        switch (len) {
            case 1:
                data[0] = byte(code);
                break;
            case 2:
                data[0] = byte((code >> 6) & b[2]) | n[1];
                data[1] = byte(code & b[1]) | n[0];
                break;
            case 3:
                data[0] = byte((code >> 12) & b[3]) | n[2];
                data[1] = byte((code >> 6) & b[1]) | n[0];
                data[2] = byte(code & b[1]) | n[0];
                break;
            case 4:
                data[0] = byte((code >> 18) & b[4]) | n[3];
                data[1] = byte((code >> 12) & b[1]) | n[0];
                data[2] = byte((code >> 6) & b[1]) | n[0];
                data[3] = byte(code & b[1]) | n[0];
                break;
            default:
                return false;
        }
        return unicode::internal::output(output, data, len);
    }

    constexpr size_t is_valid_one(auto&& data, size_t offset, size_t size) noexcept {
        size_t len = 0;
        auto check_1 = [&] {
            if constexpr (sizeof(data[1]) != 1) {
                if (!in_byte_range(data[offset])) {
                    return false;
                }
            }
            if (byte(data[offset]) <= 0x7F) {
                len = 1;
                return true;
            }
            return false;
        };
        auto check_2 = [&] {
            if (valid_range(2, data[offset], data[offset + 1])) {
                len = 2;
                return true;
            }
            return false;
        };
        auto check_3 = [&] {
            if (valid_range(3, data[offset], data[offset + 1]) &&
                is_second_later_byte(data[offset + 2])) {
                len = 3;
                return true;
            }
            return false;
        };
        auto check_4 = [&] {
            if (valid_range(4, data[offset], data[offset + 1]) &&
                is_second_later_byte(data[offset + 2]) &&
                is_second_later_byte(data[offset + 3])) {
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

    constexpr std::tuple<std::uint32_t, size_t, utf::UTFError> to_utf32(auto&& input, size_t offset, size_t size, bool replace) {
        if constexpr (sizeof(input[1]) != 1) {
            if (!in_byte_range(input[offset])) {
                return {invalid_code, 0, utf::UTFError::utf8_input_range};
            }
        }
        const auto len = is_valid_one(input, offset, size);
        if (len == 0) {
            if (replace) {
                // replacement cahracter is 3 byte code but forward 1
                return {replacement_character, 1, utf::UTFError::none};
            }
            return {invalid_code, len, utf::UTFError::utf8_input_seq};
        }
        if (size < len) {
            return {invalid_code, len, utf::UTFError::utf8_input_len};
        }
        return {decode_one_unchecked(input, offset, len), len, utf::UTFError::none};
    }

    template <class T>
    constexpr utf::UTFError to_utf32(Sequencer<T>& seq, auto& output, bool replace = false) {
        size_t off = 0;
        utf::UTFError err;
        std::tie(output, off, err) = to_utf32(seq.buf.buffer, seq.rptr, seq.remain(), replace);
        if (err != utf::UTFError::none) {
            return err;
        }
        seq.rptr += off;
        return utf::UTFError::none;
    }

    constexpr std::pair<size_t, utf::UTFError> from_utf32(auto code, auto&& output, bool replace) {
        if (!is_valid_range(code)) {
            if (replace) {
                // replacement cahracter is 3 byte code but forward 1
                return {1, encode_one_unchecked(output, replacement_character, 3)
                               ? utf::UTFError::none
                               : utf::UTFError::utf8_output_limit};
            }
            return {0, utf::UTFError::utf32_input_range};
        }
        const auto len = expect_len(code);
        return {len, encode_one_unchecked(output, code, len)
                         ? utf::UTFError::none
                         : utf::UTFError::utf8_output_limit};
    }

}  // namespace utils::unicode::utf8
