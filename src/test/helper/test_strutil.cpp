/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/helper/strutil.h"
#include <view/slice.h>

constexpr bool test_endswith() {
    return utils::helper::ends_with("Hey Jude", " Jude");
}

void test_strutils() {
    constexpr bool result1 = test_endswith();
    constexpr bool result2 = utils::helper::sandwiched("(hello)", "(", ")");
    constexpr auto s = utils::view::make_ref_splitview("call a b c", " ");
    constexpr auto sz = s.size();
    constexpr auto view = s[1];
    constexpr auto c = view[0];
}

int main() {
    test_strutils();
}
