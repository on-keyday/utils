/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <unicode/utf/view.h>

#include <string>

constexpr bool test_view_utf8_to_utf32() {
    char8_t test[] = u8"𠮷野家abc";
    futils::utf::View<char8_t*, char32_t> view(test);
    futils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(U"𠮷野家abc");
}

constexpr bool test_view_utf8_to_utf16() {
    char8_t test[] = u8"𠮷野家abc";
    futils::utf::View<char8_t*, char16_t> view(test);
    futils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u"𠮷野家abc");
}

constexpr bool test_view_utf16_to_utf32() {
    char16_t test[] = u"𠮷野家abc";
    futils::utf::View<char16_t*, char32_t> view(test);
    futils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(U"𠮷野家abc");
}

constexpr bool test_view_utf16_to_utf8() {
    char16_t test[] = u"𠮷野家abc";
    futils::utf::View<char16_t*, char8_t> view(test);
    futils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u8"𠮷野家abc");
}

constexpr bool test_view_utf32_to_utf8() {
    char32_t test[] = U"𠮷野家abc";
    futils::utf::View<char32_t*, char8_t> view(test);
    futils::Sequencer<decltype(view)&> seq(view);
    view[2];
    return seq.match(u8"𠮷野家abc");
}

constexpr bool test_view_utf32_to_utf16() {
    char32_t test[] = U"𠮷野家abc";
    futils::utf::View<char32_t*, char16_t> view(test);
    futils::Sequencer<decltype(view)&> seq(view);
    return view[2] == u'野' && seq.match(u"𠮷野家abc");
}

constexpr bool test_edge_case() {
    auto test = u8"\n # 本当はこっちが正しいのですが、現状のジェネレータの限界で、下を使っています\r\n";
    futils::utf::View<const char8_t*, char32_t> view(test);
    auto seq = futils::make_ref_seq(view);
    view[41];  // touch
    seq.rptr = 2;
    return seq.match(U"# 本当はこっちが正しいのですが、現状のジェネレータの限界で、下を使っています\r\n");
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
    constexpr bool result7 = test_edge_case();
    // static_assert(result7 == true, "view edge case is incorrect");
}

void test_utfview_non_constexpr() {
    bool result1 = test_view_utf8_to_utf32();
    // static_assert(result1 == true, "view utf8 to utf32 is incorrect");
    bool result2 = test_view_utf8_to_utf16();
    // static_assert(result2 == true, "view utf8 to utf16 is incorrect");
    bool result3 = test_view_utf16_to_utf32();
    // static_assert(result3 == true, "view utf16 to utf32 is incorrect");
    bool result4 = test_view_utf16_to_utf8();
    // static_assert(result4 == true, "view utf16 to utf8 is incorrect");
    bool result5 = test_view_utf32_to_utf8();
    // static_assert(result5 == true, "view utf32 to utf8 is incorrect");
    bool result6 = test_view_utf32_to_utf16();
    // static_assert(result6 == true, "view utf32 to utf16 is incorrect");
    bool result7 = test_edge_case();
    // static_assert(result7 == true, "view edge case is incorrect");
}

int main() {
    test_utfview();
    test_utfview_non_constexpr();
}
