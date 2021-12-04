
#include "../include/unicode/internal/fallback_sequence.h"

void test_fallback() {
    char8_t testword[] = u8"𠮷";
    utils::Sequencer seq(testword);

    utils::utf::internal::fallback(seq);
}

int main() {}