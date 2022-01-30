/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <number/radix.h>

template <class T = size_t>
void test_digitcount() {
#define RAD 16
    constexpr auto v = utils::number::digit_bound<T>[RAD];
    constexpr auto d = v[0];
    constexpr auto size_ = sizeof(utils::number::digit_bound<T>);
    static_assert(d == RAD, "expect 10 but not");
}

int main() {
    test_digitcount();
}