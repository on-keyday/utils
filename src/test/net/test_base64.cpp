/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/base64.h"

auto test_encode() {
    utils::helper::FixedPushBacker<char[64], 64> buf;
    utils::net::base64::encode("hello", buf);
    return buf;
}

auto test_decode(auto& in) {
    utils::helper::FixedPushBacker<char[5], 5> buf;
    utils::net::base64::decode(in, buf);
    return buf;
}

void test_base64() {
    using namespace utils::net;
    auto result = test_encode();
    auto result2 = test_decode(result);
}

int main() {
    test_base64();
}
