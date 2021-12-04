
#include "../include/unicode/internal/fallback_sequence.h"

constexpr bool test_fallback_utf8() {
    char8_t testword[] = u8"𠮷野家";
    utils::Sequencer seq(testword);
    seq.seekend();
    utils::utf::internal::fallback(seq);
    utils::utf::internal::fallback(seq);
    utils::utf::internal::fallback(seq);
    return seq.match(u8"𠮷野家");
}

constexpr bool test_fallback_utf16() {
    char16_t testword[] = u"𠮷野家";
    return false;
}

void test_fallback() {
    auto result = test_fallback_utf8();
}

int main() {
    test_fallback();
}
