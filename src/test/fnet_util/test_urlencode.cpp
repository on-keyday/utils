/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/util/urlencode.h>
#include <strutil/equal.h>

constexpr auto test_encode() {
    utils::helper::FixedPushBacker<char[120], 119> buf;
    utils::urlenc::encode(u8"https://ja.wikipedia.org/wiki/メインページ", buf, utils::urlenc::encodeURI());
    return buf;
}

constexpr auto test_decode() {
    utils::helper::FixedPushBacker<char8_t[120], 119> buf;
    auto res = utils::urlenc::decode("https://ja.wikipedia.org/wiki/%E3%83%A1%E3%82%A4%E3%83%B3%E3%83%9A%E3%83%BC%E3%82%B8", buf);
    return buf;
}

void test_urlencode() {
    constexpr auto result = test_encode();
    constexpr char cmp[] = "https://ja.wikipedia.org/wiki/%E3%83%A1%E3%82%A4%E3%83%B3%E3%83%9A%E3%83%BC%E3%82%B8";
    static_assert(utils::strutil::equal(result.buf, cmp, utils::strutil::ignore_case()), "why failed?");
    constexpr auto result2 = test_decode();
    constexpr auto sz = result2.count;
    constexpr char8_t cmp2[] = u8"https://ja.wikipedia.org/wiki/メインページ";
    static_assert(utils::strutil::equal(result2.buf, cmp2), "why failed?");
}

int main() {
    test_urlencode();
}