/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <number/radix.h>
#include <number/insert_space.h>

constexpr auto digit_value() {
    utils::number::Array<63, char> value{};
    utils::number::insert_space(value, 4, 0xff);
    return value;
}

void test_digitcount() {
    using T = size_t;
    constexpr size_t rad = 10;
    constexpr auto v = utils::number::digit_bound<T>[rad];
    constexpr auto d = v[0];
    constexpr auto size_ = sizeof(utils::number::digit_bound<T>);
    static_assert(d == rad, "expect 10 but not");
    constexpr auto val = digit_value();
}

int main() {
    test_digitcount();
}
