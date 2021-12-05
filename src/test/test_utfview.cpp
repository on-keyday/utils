/*license*/

#include "../include/unicode/view.h"

#include <string>

constexpr bool test_view_utf8_to_utf32() {
    char8_t test[] = u8"𠮷野家";
    utils::utf::View<char8_t*, char32_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    return seq.match(U"𠮷野家");
}

constexpr bool test_view_utf8_to_utf16() {
    char8_t test[] = u8"𠮷野家";
    utils::utf::View<char8_t*, char16_t> view(test);
    utils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u"𠮷野家");
}

void test_utfview() {
    constexpr bool result1 = test_view_utf8_to_utf32();
    bool result2 = test_view_utf8_to_utf16();
}

int main() {
    test_utfview();
}