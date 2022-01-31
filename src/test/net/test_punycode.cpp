/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net_util/punycode.h"
#include "../../include/helper/pushbacker.h"
#include "../../include/helper/equal.h"

void test_punycode() {
    utils::helper::FixedPushBacker<char[50], 49> buf;
    utils::helper::FixedPushBacker<char8_t[50], 49> buf2;
    utils::net::punycode::encode(u8"ウィキペディア", buf);
    assert(utils::helper::equal(buf.buf, "cckbak0byl6e"));
    utils::net::punycode::decode(buf.buf, buf2);
    assert(utils::helper::equal(buf2.buf, u8"ウィキペディア"));
    buf = {}, buf2 = {};
    utils::net::punycode::encode(u8"bücher", buf);
    assert(utils::helper::equal(buf.buf, "bcher-kva"));
    utils::net::punycode::decode(buf.buf, buf2);
    assert(utils::helper::equal(buf2.buf, u8"bücher"));
    buf = {}, buf2 = {};
    utils::net::punycode::encode_host(u8"Go言語.com", buf);
    assert(utils::helper::equal(buf.buf, u8"xn--go-hh0g6u.com", utils::helper::ignore_case()));
    utils::net::punycode::decode_host(buf.buf, buf2);
    assert(utils::helper::equal(buf2.buf, u8"Go言語.com", utils::helper::ignore_case()));
}

int main() {
    test_punycode();
}
