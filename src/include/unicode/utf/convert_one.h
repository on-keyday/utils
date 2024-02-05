/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// convert - converter
#pragma once
#include "utf8.h"
#include "utf16.h"
#include <tuple>

namespace futils::unicode::utf {

    constexpr std::pair<size_t, UTFError> utf8_to_utf32_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        auto [code, len, ok] = utf8::to_utf32(input, offset, size, replace);
        if (ok != UTFError::none) {
            return {len, ok};
        }
        return {len, unicode::internal::output(output, &code, 1)
                         ? UTFError::none
                         : UTFError::utf32_output_limit};
    }

    constexpr std::pair<size_t, UTFError> utf32_to_utf8_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        if (size == 0) {
            return {0, UTFError::utf32_input_len};
        }
        auto [u8len, ok] = utf8::from_utf32(input[offset], output, replace);
        return {u8len ? 1 : 0, ok};
    }

    constexpr std::pair<size_t, UTFError> utf16_to_utf32_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        auto [code, len, ok] = utf16::to_utf32(input, offset, size, replace);
        if (ok != UTFError::none) {
            return {len, ok};
        }
        return {len, unicode::internal::output(output, &code, 1)
                         ? UTFError::none
                         : UTFError::utf32_output_limit};
    }

    constexpr std::pair<size_t, UTFError> utf32_to_utf16_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        if (size == 0) {
            return {0, UTFError::utf32_input_len};
        }
        auto [u16len, ok] = utf16::from_utf32(input[offset], output, replace);
        return {u16len ? 1 : 0, ok};
    }

    constexpr std::pair<size_t, UTFError> utf8_to_utf16_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        auto [code, len, ok] = utf8::to_utf32(input, offset, size, replace);
        if (ok != UTFError::none) {
            return {len, ok};
        }
        return {len, utf16::encode_one_unchecked(output, code, utf16::expect_len(code))
                         ? UTFError::none
                         : UTFError::utf16_output_limit};
    }

    constexpr std::pair<size_t, UTFError> utf16_to_utf8_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        auto [code, len, ok] = utf16::to_utf32(input, offset, size, replace);
        if (ok != UTFError::none) {
            return {len, ok};
        }
        return {len, utf8::encode_one_unchecked(output, code, utf8::expect_len(code))
                         ? UTFError::none
                         : UTFError::utf8_output_limit};
    }

    constexpr std::pair<size_t, UTFError> utf8_to_utf8_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        auto [code, len, ok] = utf8::to_utf32(input, offset, size, replace);
        if (ok != UTFError::none) {
            return {len, ok};
        }
        return {len, utf8::encode_one_unchecked(output, code, utf8::expect_len(code))
                         ? UTFError::none
                         : UTFError::utf16_output_limit};
    }

    constexpr std::pair<size_t, UTFError> utf16_to_utf16_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        auto [code, len, ok] = utf16::to_utf32(input, offset, size, replace);
        if (ok != UTFError::none) {
            return {len, ok};
        }
        return {len, utf16::encode_one_unchecked(output, code, utf16::expect_len(code))
                         ? UTFError::none
                         : UTFError::utf8_output_limit};
    }

    constexpr std::pair<size_t, UTFError> utf32_to_utf32_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        if (size == 0) {
            return {0, UTFError::utf32_input_len};
        }
        std::uint32_t code;
        if (!is_valid_range(input[offset])) {
            if (!replace) {
                return {0, UTFError::utf32_input_range};
            }
            code = replacement_character;
        }
        else {
            code = input[offset];
        }
        return {1, unicode::internal::output(output, &code, 1)
                       ? UTFError::none
                       : UTFError::utf32_output_limit};
    }

}  // namespace futils::unicode::utf
