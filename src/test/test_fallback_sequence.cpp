
#include "../include/unicode/internal/fallback_sequence.h"

constexpr bool test_fallback_utf8() {
    char8_t testword[] = u8"𠮷野";
    utils::Sequencer seq(testword);
    seq.seekend();
    utils::utf::internal::fallback(seq);
    return seq.match(u8"野");
}

void test_fallback() {
    constexpr auto result = test_fallback_utf8();
}

int main() {
    test_fallback();
}