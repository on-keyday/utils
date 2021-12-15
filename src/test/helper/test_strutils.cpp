/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/helper/strutil.h"

constexpr bool test_endswith() {
    return utils::helper::ends_with("Hey Jude", " Jude");
}

void test_strutils() {
    constexpr bool result1 = test_endswith();
    constexpr bool result2 = utils::helper::sandwiched("(hello)", "(", ")");
}
