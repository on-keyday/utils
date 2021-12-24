/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/number/to_string.h"

#include "../../include/helper/pushbacker.h"

#include "../../include/helper/strutil.h"

constexpr auto test_to_string_num(int i, int radix = 10) {
    utils::helper::FixedPushBacker<char[66], 65> buf;
    utils::number::to_string(buf, i, radix);
    return buf;
}

void test_to_string() {
    constexpr auto result = test_to_string_num(10);
    static_assert(utils::helper::equal(result.buf, "10"), "expect 10 but assetion failed");
    constexpr auto result2 = test_to_string_num(-12394);
    static_assert(utils::helper::equal(result2.buf, "-12394"), "expect -12394 but assetion failed");
    constexpr auto result3 = test_to_string_num(0xff, 16);
    static_assert(utils::helper::equal(result3.buf, "ff"), "expect ff but assetion failed");
}

int main() {
    test_to_string();
}