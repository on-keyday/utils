/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// error
#pragma once

namespace utils::unicode::utf {
    enum class UTFError {
        none,
        utf8_input_range,    // input<0||0xff<input
        utf8_input_seq,      // not vaild input sequence
        utf8_input_len,      // not vaild input length
        utf8_output_limit,   // output limit reached
        utf16_input_range,   // input<0||0xff<input
        utf16_input_seq,     // not vaild input sequence
        utf16_input_len,     // not vaild input length
        utf16_output_limit,  // output limit reached
        utf32_input_range,   // input<0||0x10ffff<input
        utf32_input_len,     // not vaild input length
        utf32_output_limit,  // output limit reached
        utf_unknown,         // unkonw utf type
        utf_output_limit,
        utf_no_input,
    };

}  // namespace utils::unicode::utf
