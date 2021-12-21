/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/base64.h"

constexpr auto test_encode() {
    utils::helper::FixedPushBacker<char[64], 64> buf;
    utils::net::base64::encode("hello", buf);
    return buf;
}

void test_base64() {
    using namespace utils::net;
    constexpr auto result = test_encode();
}

int main() {
    test_base64();
}
