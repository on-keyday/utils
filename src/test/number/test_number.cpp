/*license*/

#include "../../include/number/number.h"

constexpr bool test_is_number() {
    bool is_float;
    return utils::number::is_number("3", 16, 0, &is_float);
}

constexpr int test_parse_integer() {
    int test = 0;
    utils::number::parse_integer("-92013827", test);
    return test;
}

constexpr double test_parse_float() {
    double test = 0;
    utils::number::parse_float("3", test);
    return test;
}

void test_number() {
    constexpr bool result1 = test_is_number();
    constexpr auto result2 = test_parse_integer();
    constexpr auto result3 = test_parse_float();
}

int main() {
    test_number();
}
