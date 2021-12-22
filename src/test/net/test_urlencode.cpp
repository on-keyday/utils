/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/urlencode.h"
#include "../../include/helper/strutil.h"

constexpr auto test_encode() {
    utils::helper::FixedPushBacker<char[120], 119> buf;
    utils::net::urlenc::encode(u8"https://ja.wikipedia.org/wiki/メインページ", buf, utils::net::urlenc::encodeURI());
    return buf;
}

void test_urlencode() {
    constexpr auto result = test_encode();
    //assert(utils::helper::equal(result.buf, "https://ja.wikipedia.org/wiki/%E3%83%A1%E3%82%A4%E3%83%B3%E3%83%9A%E3%83%BC%E3%82%B8"));
}

int main() {
    test_urlencode();
}