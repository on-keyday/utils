/*license*/

#include "../include/unicode/conv_method.h"

char32_t test_utf_convert_constexpr() {
    char8_t testword[] = u8"𠮷";
    char32_t result = 0;
    utils::utf::utf8_to_utf32(testword, result);
    return result;
}

void test_utf_convert() {
    auto result = test_utf_convert_constexpr();
}

int main() {
    test_utf_convert();
}