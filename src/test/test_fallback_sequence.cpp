/*license*/

#include "../include/unicode/internal/fallback_sequence.h"

constexpr bool test_fallback_utf8() {
    char8_t testword[] = u8"𠮷野家𠮷";
    utils::Sequencer seq(testword);
    seq.seekend();
    utils::utf::internal::fallback(seq);
    utils::utf::internal::fallback(seq);
    utils::utf::internal::fallback(seq);
    return seq.match(u8"野家𠮷");
}

constexpr bool test_fallback_utf16() {
    char16_t testword[] = u"𠮷野家𠮷";
    utils::Sequencer seq(testword);
    seq.seekend();
    utils::utf::internal::fallback(seq);
    utils::utf::internal::fallback(seq);
    utils::utf::internal::fallback(seq);
    return seq.match(u"野家𠮷");
}

void test_fallback() {
    constexpr auto result1 = test_fallback_utf8();
    static_assert(result1 == true, "fallback for utf8 is wrong");
    constexpr auto result2 = test_fallback_utf16();
    static_assert(result2 == true, "fallback for utf16 is wrong");
}

int main() {
    test_fallback();
}
