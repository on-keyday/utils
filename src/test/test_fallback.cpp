
#include "../include/unicode/internal/fallback_sequence.h"

constexpr size_t test_fallback() {
    char8_t testword[] = u8"𠮷";
    utils::Sequencer seq(testword);
    seq.seekend();
    utils::utf::internal::fallback(seq);
    return seq.rptr;
}

int main() {
    constexpr auto result = test_fallback();
}