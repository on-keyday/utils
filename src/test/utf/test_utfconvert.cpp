/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/unicode/utf/convert.h"
#include "../../include/unicode/utf/minibuffer.h"
#include <string>

constexpr char32_t test_utf8_to_utf32() {
    char8_t testword[] = u8"𠮷";
    return futils::unicode::utf8::decode_one_unchecked(testword, 0, futils::unicode::utf8::first_byte_to_len(testword[0]));
}

constexpr char32_t test_utf16_to_utf32() {
    char16_t testword[] = u"𠮷";
    return futils::unicode::utf16::decode_one_unchecked(testword, 0, 2);
}

constexpr futils::utf::MiniBuffer<char8_t> test_utf32_to_utf8() {
    char32_t testword = U'𠮷';
    futils::utf::MiniBuffer<char8_t> result;
    futils::unicode::utf8::encode_one_unchecked(result, testword, 4);
    return result;
}

constexpr futils::utf::MiniBuffer<char16_t> test_utf32_to_utf16() {
    char32_t testword = U'𠮷';
    futils::utf::MiniBuffer<char16_t> result;
    futils::unicode::utf16::encode_one_unchecked(result, testword, 2);
    return result;
}

constexpr futils::utf::MiniBuffer<char16_t> test_convert() {
    futils::utf::MiniBuffer<char16_t> result;
    futils::utf::convert(u8"𠮷", result);
    return result;
}

void test_utf_convert() {
    constexpr auto result1 = test_utf8_to_utf32();
    static_assert(result1 == U'𠮷', "utf8_to_utf32 is wrong");
    constexpr auto result2 = test_utf16_to_utf32();
    static_assert(result2 == U'𠮷', "utf16_to_utf32 is wrong");
    constexpr auto expect_result3 = futils::utf::MiniBuffer<char8_t>{u8"𠮷"};
    constexpr auto result3 = test_utf32_to_utf8();
    static_assert(result3 == expect_result3, "utf32_to_utf8 is wrong");
    constexpr auto expect_result4 = futils::utf::MiniBuffer<char16_t>{u"𠮷"};
    constexpr auto result4 = test_utf32_to_utf16();
    static_assert(result4 == expect_result4, "utf32_to_utf16 is wrong");
    constexpr auto expect_result5 = result4;
    constexpr auto result5 = test_convert();
    static_assert(result5 == expect_result5, "convert is wrong");
}

int main() {
    test_utf_convert();
}
