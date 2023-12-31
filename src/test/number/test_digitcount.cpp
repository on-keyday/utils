/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <number/radix.h>
#include <number/insert_space.h>
#include <wrap/cout.h>
#include <iomanip>

constexpr auto digit_value() {
    utils::number::Array<char, 63> value{};
    utils::number::insert_space(value, 4, 0xff);
    return value;
}

void test_digitcount() {
    using T = size_t;
    constexpr size_t rad = 3;
    constexpr auto v = utils::number::digit_bound<T>[rad];
    constexpr auto d = v[0];
    constexpr auto size_ = sizeof(utils::number::digit_bound<T>);
    static_assert(d == rad, "expect 10 but not");
    constexpr auto val = digit_value();
    auto ptr = reinterpret_cast<const std::uint8_t*>(utils::number::digit_bound<T>);
    for (size_t i = 0; i < size_; i++) {
        if (ptr[i] == 0) continue;
        utils::wrap::cout_wrap() << std::setfill(TO_TCHAR('0')) << std::setw(2) << std::hex << size_t(ptr[i]) << ",";
    }
}

int main() {
    test_digitcount();
}
