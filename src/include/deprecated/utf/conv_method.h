/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// conv_method - utf conversion method implementation
#pragma once

#include "../../core/sequencer.h"
#include "minibuffer.h"
#include "internal/conv_method_helper.h"
#include "../../wrap/light/enum.h"

namespace utils {
    namespace utf {

        enum class UTFError {
            none,
            utf8_no_input,
            utf16_no_input,
            utf8_input_length,
            utf16_input_length,
            utf8_first_byte_mask,
            utf8_illformed_sequence,
            utf16_num_range,
            utf16_invalid_surrogate,
            utf32_out_of_range,
            invalid_parameter,
            unknown,
        };

        constexpr const char* to_string(UTFError err) {
            switch (err) {
                case UTFError::none:
                    return nullptr;
                case UTFError::utf8_no_input:
                    return "no input of utf8 sequence";
                case UTFError::utf16_no_input:
                    return "no input of utf16 sequence";
                case UTFError::utf8_input_length:
                    return "not enough input for expected length utf8 sequence";
                case UTFError::utf16_input_length:
                    return "not enough input for utf16 surrogate pair";
                case UTFError::utf8_first_byte_mask:
                    return "unexpected bits for utf8 first byte";
                case UTFError::utf8_illformed_sequence:
                    return "illformed sequence for expected length utf8";
                case UTFError::utf16_num_range:
                    return "input is greater than 0xffff on utf16";
                case UTFError::utf16_invalid_surrogate:
                    return "expect low surrogate but unexpected chararcter";
                case UTFError::utf32_out_of_range:
                    return "input is greater than 0x10ffff on utf32";
                case UTFError::invalid_parameter:
                    return "invalid parameter for utf:convert_one";
                default:
                    return "unknown error";
            }
        }

        using UTFErr = wrap::EnumWrap<UTFError, UTFError::none, UTFError::unknown>;

        template <class C>
        constexpr bool is_utf16_high_surrogate(C c) {
            return 0xD800 <= c && c <= 0xDC00;
        }

        template <class C>
        constexpr bool is_utf16_low_surrogate(C c) {
            return 0xDC00 <= c && c < 0xE000;
        }

        template <class C>
        constexpr std::uint8_t utf8_expect_len_from_first_byte(C in) {
            std::make_unsigned_t<C> c = in;
            if (c < 0 || c > 0xff) {
                return 0;
            }
            if (c >= 0 && c <= 0x7f) {
                return 1;
            }
            else if (is_mask_of<2>(c)) {
                return 2;
            }
            else if (is_mask_of<3>(c)) {
                return 3;
            }
            else if (is_mask_of<4>(c)) {
                return 4;
            }
            return 0;
        }

        constexpr bool is_utf8_second_later_byte(auto b) {
            return 0x80 <= b && b <= 0xBF;
        }

        constexpr bool is_valid_range(std::uint8_t len, std::uint8_t one, std::uint8_t two) {
            auto normal_two_range = [&] {
                return is_utf8_second_later_byte(two);
            };
            switch (len) {
                case 1:
                    return true;
                case 2:
                    return 0xC2 <= one && one <= 0xDF &&
                           normal_two_range();
                case 3:
                    if (one == 0xE0) {
                        return 0xA0 <= two && two <= 0xBF;
                    }
                    else if (one == 0xED) {
                        return 0x80 <= two && two <= 0x9F;
                    }
                    return 0xE0 <= one && one <= 0xEF &&
                           normal_two_range();
                case 4:
                    if (one == 0xF0) {
                        return 0x90 <= two && two <= 0xBF;
                    }
                    else if (one == 0xF4) {
                        return 0x80 <= two && two <= 0x8F;
                    }
                    return 0xF0 <= one && one <= 0xF4 &&
                           normal_two_range();
                default:
                    return false;
            }
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf8_to_utf32(Sequencer<T>& input, C& output) {
            using unsigned_t = std::make_unsigned_t<typename Sequencer<T>::char_type>;
            if (input.eos()) {
                return UTFError::utf8_no_input;
            }
            auto ofs = [&](size_t ofs) {
                return static_cast<unsigned_t>(input.current(ofs));
            };
            auto in = ofs(0);
            auto len = utf8_expect_len_from_first_byte(in);
            if (len == 0) {
                return UTFError::utf8_first_byte_mask;
            }
            if (len == 1) {
                input.consume();
                output = static_cast<C>(in);
                return true;
            }
            if (input.remain() < len) {
                return UTFError::utf8_input_length;
            }
            if (!is_valid_range(len, ofs(0), ofs(1))) {
                return UTFError::utf8_illformed_sequence;
            }
            if (len >= 3) {
                if (!is_utf8_second_later_byte(ofs(2))) {
                    return UTFError::utf8_illformed_sequence;
                }
            }
            if (len >= 4) {
                if (!is_utf8_second_later_byte(ofs(3))) {
                    return UTFError::utf8_illformed_sequence;
                }
            }
            internal::make_utf32_from_utf8(len, input.buf.buffer, output, input.rptr);
            input.consume(len);
            return true;
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf8_to_utf32(T&& input, C& output) {
            Sequencer<buffer_t<T&>> seq(input);
            return utf8_to_utf32(seq, output);
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf16_to_utf32(Sequencer<T>& input, C& output) {
            using unsigned_t = std::make_unsigned_t<typename Sequencer<T>::char_type>;
            if (input.eos()) {
                return UTFError::utf16_no_input;
            }
            auto ofs = [&](size_t ofs) {
                return static_cast<unsigned_t>(input.current(ofs));
            };
            auto in = ofs(0);
            if (in > 0xffff) {
                return UTFError::utf16_num_range;
            }
            if (is_utf16_high_surrogate(in)) {
                if (input.remain() < 2) {
                    return UTFError::utf16_input_length;
                }
                auto sec = ofs(1);
                if (!is_utf16_low_surrogate(sec)) {
                    return UTFError::utf16_invalid_surrogate;
                }
                output = internal::make_surrogate_char<unsigned_t, C>(in, sec);
                input.consume(2);
            }
            else {
                output = static_cast<C>(in);
                input.consume();
            }
            return true;
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf16_to_utf32(T&& input, C& output) {
            Sequencer<buffer_t<T&>> seq(input);
            return utf16_to_utf32(seq, output);
        }

        constexpr size_t utf32_expect_utf8_len(auto c) {
            if (c < 0) {
                return 0;
            }
            else if (c < 0x80) {
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

        template <class T, class C>
        constexpr UTFErr utf32_to_utf8(C input, T& output) {
            using unsigned_t = std::make_unsigned_t<C>;
            auto in = static_cast<unsigned_t>(input);
            auto len = utf32_expect_utf8_len(in);
            if (len == 1) {
                output.push_back(in);
            }
            else if (len == 2) {
                internal::make_utf8_from_utf32<2>(in, output);
            }
            else if (len == 3) {
                internal::make_utf8_from_utf32<3>(in, output);
            }
            else if (len == 4) {
                internal::make_utf8_from_utf32<4>(in, output);
            }
            else {
                return UTFError::utf32_out_of_range;
            }
            return true;
        }

        template <class T, class C>
        constexpr UTFErr utf32_to_utf16(C input, T& output) {
            using unsigned_t = std::make_unsigned_t<C>;
            auto in = static_cast<unsigned_t>(input);
            if (in >= 0x110000) {
                return UTFError::utf32_out_of_range;
            }
            if (in > 0xffff) {
                auto first = char16_t((in - 0x10000) / 0x400 + 0xD800);
                auto second = char16_t((in - 0x10000) % 0x400 + 0xDC00);
                output.push_back(first);
                output.push_back(second);
            }
            else {
                output.push_back(in);
            }
            return true;
        }

        template <class T, class C>
        constexpr UTFErr utf8_to_utf16(T& input, C& output) {
            char32_t tmp = 0;
            if (auto ok = utf8_to_utf32(input, tmp); !ok) {
                return ok;
            }
            return utf32_to_utf16(tmp, output);
        }

        template <class T, class C>
        constexpr UTFErr utf16_to_utf8(T& input, C& output) {
            char32_t tmp = 0;
            if (auto ok = utf16_to_utf32(input, tmp); !ok) {
                return ok;
            }
            return utf32_to_utf8(tmp, output);
        }

    }  // namespace utf
}  // namespace utils
