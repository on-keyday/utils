/*license*/

#include "../include/unicode/view.h"

#include <string>

constexpr bool test_view_utf8_to_utf32() {
    char8_t test[] = u8"𠮷野家abc";
    utils::utf::View<char8_t*, char32_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(U"𠮷野家abc");
}

constexpr bool test_view_utf8_to_utf16() {
    char8_t test[] = u8"𠮷野家abc";
    utils::utf::View<char8_t*, char16_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u"𠮷野家abc");
}

constexpr bool test_view_utf16_to_utf32() {
    char16_t test[] = u"𠮷野家abc";
    utils::utf::View<char16_t*, char32_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(U"𠮷野家abc");
}

constexpr bool test_view_utf16_to_utf8() {
    char16_t test[] = u"𠮷野家abc";
    utils::utf::View<char16_t*, char8_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u8"𠮷野家abc");
}

constexpr bool test_view_utf32_to_utf8() {
    char32_t test[] = U"𠮷野家abc";
    utils::utf::View<char32_t*, char8_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u8"𠮷野家abc");
}

constexpr bool test_view_utf32_to_utf16() {
    char32_t test[] = U"𠮷野家abc";
    utils::utf::View<char32_t*, char16_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u"𠮷野家abc");
}

void test_utfview() {
    constexpr bool result1 = test_view_utf8_to_utf32();
    static_assert(result1 == true, "view utf8 to utf32 is incorrect");
    constexpr bool result2 = test_view_utf8_to_utf16();
    static_assert(result2 == true, "view utf8 to utf16 is incorrect");
    constexpr bool result3 = test_view_utf16_to_utf32();
    static_assert(result3 == true, "view utf16 to utf32 is incorrect");
    constexpr bool result4 = test_view_utf16_to_utf8();
    static_assert(result4 == true, "view utf16 to utf8 is incorrect");
    constexpr bool result5 = test_view_utf32_to_utf8();
    static_assert(result5 == true, "view utf32 to utf8 is incorrect");
    constexpr bool result6 = test_view_utf32_to_utf16();
    static_assert(result6 == true, "view utf32 to utf16 is incorrect");
}

int main() {
    test_utfview();
}
