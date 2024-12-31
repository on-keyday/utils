/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/strutil/strutil.h"
#include <view/slice.h>
#include <strutil/ring_buffer.h>

constexpr bool test_endswith() {
    return futils::strutil::ends_with("Hey Jude", " Jude");
}

void test_strfutils() {
    constexpr bool result1 = test_endswith();
    constexpr bool result2 = futils::strutil::sandwiched("(hello)", "(", ")");
    constexpr auto s = futils::view::make_ref_splitview("call a b c", " ");
    constexpr auto sz = s.size();
    constexpr auto view = s[1];
    constexpr auto c = view[0];
}

int main() {
    test_strfutils();
}
