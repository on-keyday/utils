/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/number/to_string.h"

#include "../../include/helper/pushbacker.h"

#include "../../include/strutil/equal.h"

template <class T>
constexpr auto test_to_string_num(T i, int radix = 10) {
    utils::helper::FixedPushBacker<char[66], 65> buf;
    utils::number::to_string(buf, i, radix);
    return buf;
}

void test_to_string() {
    constexpr auto result = test_to_string_num(10);
    static_assert(utils::strutil::equal(result.buf, "10"), "expect 10 but assetion failed");
    constexpr auto result2 = test_to_string_num(-12394);
    static_assert(utils::strutil::equal(result2.buf, "-12394"), "expect -12394 but assetion failed");
    constexpr auto result3 = test_to_string_num(0xff, 16);
    static_assert(utils::strutil::equal(result3.buf, "ff"), "expect ff but assetion failed");
#if !defined(__GNUC__) || defined(__clang__)
    constexpr auto result4 = test_to_string_num(0.5);
    static_assert(utils::strutil::equal(result4.buf, "0.5"), "expect 0.5 but assetion failed");
#endif
}

int main() {
    test_to_string();
}