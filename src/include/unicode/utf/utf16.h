/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <utility>
#include "unicode.h"
#include <tuple>
#include "output.h"
#include "error.h"

namespace utils::unicode::utf16 {

    constexpr bool in_word_range(auto c) noexcept {
        return 0 <= c && c <= 0xff;
    }

    constexpr size_t expect_len(std::uint32_t code) noexcept {
        if (code <= 0xFFFF) {
            return 1;
        }
        else if (code <= 0x10FFFF) {
            return 2;
        }
        return 0;
    }

    constexpr bool is_high_surrogate(std::uint16_t c) noexcept {
        return 0xD800 <= c && c < 0xDC00;
    }

    constexpr bool is_low_surrogate(std::uint16_t c) noexcept {
        return 0xDC00 <= c && c < 0xE000;
    }

    constexpr std::uint32_t make_surrogate_char(std::uint16_t first, std::uint16_t second) noexcept {
        return 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
    }

    constexpr std::pair<std::uint16_t, std::uint16_t> split_surrogate_char(std::uint32_t code) noexcept {
        // code = 0x10000 + (first-0xD800) * 0x400 + (second-0xDC00)
        const auto r = (code - 0x10000);
        return {
            ((r >> 10) & 0x3ff) + surrogate_1,
            (r & 0x3ff) + surrogate_2,
        };
    }

    constexpr std::uint16_t utf16_bom = byte_order_mark;
    constexpr std::uint16_t utf16_repchar = replacement_character;

    constexpr std::uint32_t decode_one_unchecked(auto&& data, size_t offset, size_t len) noexcept(noexcept(data[1])) {
        switch (len) {
            case 1:
                return data[offset];
            case 2:
                return make_surrogate_char(data[offset], data[offset + 1]);
            default:
                return invalid_code;
        }
    }

    constexpr bool encode_one_unchecked(auto&& output, std::uint32_t code, size_t len) {
        std::uint16_t c[2]{};
        switch (len) {
            case 1:
                c[0] = std::uint16_t(code);
                break;
            case 2:
                std::tie(c[0], c[1]) = split_surrogate_char(code);
                break;
            default:
                return false;
        }
        return unicode::internal::output(output, c, len);
    }

    constexpr size_t is_valid_one(auto&& data, size_t offset, size_t size) noexcept {
        switch (size) {
            case 0:
                return 0;
            case 1:
                if (is_high_surrogate(data[offset]) ||
                    is_low_surrogate(data[offset])) {
                    return 0;
                }
                if constexpr (sizeof(data[1]) > 2) {
                    return std::uint16_t(data[offset]) > 0xffff ? 0 : 1;
                }
                return 1;
            default:
                if (is_high_surrogate(data[offset])) {
                    if (!is_low_surrogate(data[offset + 1])) {
                        return 0;
                    }
                    return 2;
                }
                else if (is_low_surrogate(data[offset])) {
                    return 0;
                }
                if constexpr (sizeof(data[1]) > 2) {
                    return std::uint16_t(data[offset]) > 0xffff ? 0 : 1;
                }
                return 1;
        }
    }

    constexpr std::tuple<std::uint32_t, size_t, utf::UTFError> to_utf32(auto&& input, size_t offset, size_t size, bool replace) {
        if constexpr (sizeof(input[1]) > 2) {
            if (!in_word_range(input[offset])) {
                return {invalid_code, 0, utf::UTFError::utf16_input_range};
            }
        }
        const auto len = is_valid_one(input, offset, size);
        if (len == 0) {
            if (replace) {
                return {replacement_character, 1, utf::UTFError::none};
            }
            return {invalid_code, len, utf::UTFError::utf16_input_seq};
        }
        if (size < len) {
            return {invalid_code, len, utf::UTFError::utf16_input_len};
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
                return {1, encode_one_unchecked(output, replacement_character, 1)
                               ? utf::UTFError::none
                               : utf::UTFError::utf16_output_limit};
            }
            return {0, utf::UTFError::utf32_input_range};
        }
        const auto len = expect_len(code);
        return {len, encode_one_unchecked(output, code, len)
                         ? utf::UTFError::none
                         : utf::UTFError::utf16_output_limit};
    }

}  // namespace utils::unicode::utf16
