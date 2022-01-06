/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/operator/utf_convert.h"

#include <string>

void test_utf_operator() {
    std::u8string str;
    std::u16string str2;
    utils::ops::from(U"Hello World! こんにちは世界") >> str;
    assert(str == u8"Hello World! こんにちは世界" && "operator is broken");
    utils::ops::from(str) >> str2;
    assert(str2 == u"Hello World! こんにちは世界" && "operator is broken");
}

int main() {
    test_utf_operator();
}
