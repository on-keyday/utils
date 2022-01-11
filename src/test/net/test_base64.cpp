/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/base64.h"
#include "../../include/helper/equal.h"
#include "../../include/helper/pushbacker.h"

auto test_encode(auto& in) {
    utils::helper::FixedPushBacker<char[120], 119> buf;
    utils::net::base64::encode(in, buf);
    return buf;
}

auto test_decode(auto& in) {
    utils::helper::FixedPushBacker<char[120], 119> buf;
    utils::net::base64::decode(in, buf);
    return buf;
}

void test_base64() {
    using namespace utils::net;
    auto result = test_encode("hello");
    auto result2 = test_decode(result.buf);
    assert(utils::helper::equal(result2.buf, "hello"));
}

int main() {
    test_base64();
}
