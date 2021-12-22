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

constexpr auto test_decode() {
    utils::helper::FixedPushBacker<char8_t[120], 119> buf;
    auto res = utils::net::urlenc::decode("https://ja.wikipedia.org/wiki/%E3%83%A1%E3%82%A4%E3%83%B3%E3%83%9A%E3%83%BC%E3%82%B8", buf);
    return buf;
}

void test_urlencode() {
    constexpr auto result = test_encode();
    constexpr char cmp[] = "https://ja.wikipedia.org/wiki/%E3%83%A1%E3%82%A4%E3%83%B3%E3%83%9A%E3%83%BC%E3%82%B8";
    static_assert(utils::helper::equal(result.buf, cmp, utils::helper::ignore_case()) && "why failed?");
    auto result2 = test_decode();
    /*constexpr auto sz = result2.count;
    constexpr char8_t cmp2[] = u8"https://ja.wikipedia.org/wiki/メインページ";
    static_assert(utils::helper::equal(result2.buf, cmp2), "why failed?");*/
}

int main() {
    test_urlencode();
}