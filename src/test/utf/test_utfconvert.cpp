/*license*/

#include "../../include/utf/conv_method.h"
#include "../../include/utf/convert.h"
#include <string>

constexpr char32_t test_utf8_to_utf32() {
    char8_t testword[] = u8"𠮷";
    char32_t result = 0;
    utils::utf::utf8_to_utf32(testword, result);
    return result;
}

constexpr char32_t test_utf16_to_utf32() {
    char16_t testword[] = u"𠮷";
    char32_t result = 0;
    utils::utf::utf16_to_utf32(testword, result);
    return result;
}

constexpr utils::utf::Minibuffer<char8_t> test_utf32_to_utf8() {
    char32_t testword = U'𠮷';
    utils::utf::Minibuffer<char8_t> result;
    utils::utf::utf32_to_utf8(testword, result);
    return result;
}

constexpr utils::utf::Minibuffer<char16_t> test_utf32_to_utf16() {
    char32_t testword = U'𠮷';
    utils::utf::Minibuffer<char16_t> result;
    utils::utf::utf32_to_utf16(testword, result);
    return result;
}

constexpr utils::utf::Minibuffer<char16_t> test_convert() {
    utils::utf::Minibuffer<char16_t> result;
    utils::utf::convert(u8"𠮷", result);
    return result;
}

void test_utf_convert() {
    constexpr auto result1 = test_utf8_to_utf32();
    static_assert(result1 == U'𠮷', "utf8_to_utf32 is wrong");
    constexpr auto result2 = test_utf16_to_utf32();
    static_assert(result2 == U'𠮷', "utf16_to_utf32 is wrong");
    constexpr auto expect_result3 = utils::utf::Minibuffer<char8_t>{u8"𠮷"};
    constexpr auto result3 = test_utf32_to_utf8();
    static_assert(result3 == expect_result3, "utf32_to_utf8 is wrong");
    constexpr auto expect_result4 = utils::utf::Minibuffer<char16_t>{u"𠮷"};
    constexpr auto result4 = test_utf32_to_utf16();
    static_assert(result4 == expect_result4, "utf32_to_utf16 is wrong");
    constexpr auto expect_result5 = result4;
    constexpr auto result5 = test_convert();
    static_assert(result5 == expect_result5, "convert is wrong");
}

int main() {
    test_utf_convert();
}
