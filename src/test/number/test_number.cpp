/*license*/

#include "../../include/number/number.h"

constexpr bool test_is_number() {
    return utils::number::is_number("92");
}

void test_number() {
    bool result1 = test_is_number();
}

int main() {
    test_number();
}