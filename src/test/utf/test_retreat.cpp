/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/unicode/utf/retreat.h"

constexpr bool test_retreat_utf8() {
    char8_t testword[] = u8"𠮷野家𠮷";
    futils::Sequencer seq(testword);
    seq.seekend();
    futils::utf::internal::retreat(seq);
    futils::utf::internal::retreat(seq);
    futils::utf::internal::retreat(seq);
    return seq.match(u8"野家𠮷");
}

constexpr bool test_retreat_utf16() {
    char16_t testword[] = u"𠮷野家𠮷";
    futils::Sequencer seq(testword);
    seq.seekend();
    futils::utf::internal::retreat(seq);
    futils::utf::internal::retreat(seq);
    futils::utf::internal::retreat(seq);
    return seq.match(u"野家𠮷");
}

constexpr bool test_retreat_utf32() {
    char32_t testword[] = U"𠮷野家𠮷";
    futils::Sequencer seq(testword);
    seq.seekend();
    futils::utf::internal::retreat(seq);
    futils::utf::internal::retreat(seq);
    futils::utf::internal::retreat(seq);
    return seq.match(U"野家𠮷");
}

constexpr bool test_retreat_edge_case() {
    auto test = u8" # 本当はこっちが正しいのですがΩ、現状のジェネレータの限界で、下を使っています\n";
    futils::Sequencer s(test);
    s.rptr = 117;
    for (auto i = 0; i < 41; i++) {
        futils::utf::internal::retreat(s);
    }
    return s.match(u8"# 本当はこっちが正しいのですがΩ、現状のジェネレータの限界で、下を使っています\n");
}

void test_retreat() {
    constexpr auto result1 = test_retreat_utf8();
    static_assert(result1 == true, "retreat for utf8 is wrong");
    constexpr auto result2 = test_retreat_utf16();
    static_assert(result2 == true, "retreat for utf16 is wrong");
    constexpr auto result3 = test_retreat_utf32();
    static_assert(result3 == true, "retreat for utf32 is wrong");
    constexpr auto result4 = test_retreat_edge_case();
    static_assert(result4 == true, "retreat for edge case is wrong");
}

void test_retreat_non_constexpr() {
    auto result1 = test_retreat_utf8();
    assert(result1 == true);
    auto result2 = test_retreat_utf16();
    assert(result2 == true);
    auto result3 = test_retreat_utf32();
    assert(result3 == true);
    auto result4 = test_retreat_edge_case();
    assert(result4 == true);
}

int main() {
    test_retreat();
    test_retreat_non_constexpr();
}
