/*license*/

#include "../include/unicode/conv_method.h"

constexpr char32_t test_utf_convert_constexpr() {
    char8_t testword[] = u8"𠮷";
    char32_t result = 0;
    utils::utf::utf8_to_utf32(testword, result);
    return result;
}

constexpr void test_utf_convert() {
    constexpr auto result = test_utf_convert_constexpr();
}

int main() {
    test_utf_convert();
}