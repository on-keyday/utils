/*license*/

#include "../../include/number/number.h"

constexpr bool test_is_number() {
    bool is_float;
    return utils::number::is_number("01101101", 2, &is_float);
}

void test_number() {
    constexpr bool result1 = test_is_number();
}

int main() {
    test_number();
}