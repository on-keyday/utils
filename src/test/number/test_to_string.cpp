/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/number/to_string.h"

#include "../../include/helper/pushbacker.h"

constexpr auto test_to_string_num(int i) {
    utils::helper::FixedPushBacker<char[66], 65> buf;
    utils::number::to_string(buf, i);
    return buf;
}

void test_to_string() {
    auto result = test_to_string_num(10);
}

int main() {
    test_to_string();
}