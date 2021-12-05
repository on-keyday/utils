/*license*/

#include "../include/unicode/view.h"

#include <string>

constexpr bool test_view_utf8_to_utf32() {
    char8_t test[] = u8"𠮷野家";
    utils::utf::View<char8_t*, char32_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(U"𠮷野家");
}

constexpr bool test_view_utf8_to_utf16() {
    char8_t test[] = u8"𠮷野家";
    utils::utf::View<char8_t*, char16_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u"𠮷野家");
}

constexpr bool test_view_utf16_to_utf32() {
    char16_t test[] = u"𠮷野家";
    utils::utf::View<char16_t*, char32_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(U"𠮷野家");
}

void test_utfview() {
    constexpr bool result1 = test_view_utf8_to_utf32();
    constexpr bool result2 = test_view_utf8_to_utf16();
    bool result3 = test_view_utf16_to_utf32();
}

int main() {
    test_utfview();
}
