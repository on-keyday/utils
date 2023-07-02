/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/util/punycode.h>
#include <helper/pushbacker.h>
#include <strutil/equal.h>

void test_punycode() {
    utils::helper::FixedPushBacker<char[50], 49> buf;
    utils::helper::FixedPushBacker<char8_t[50], 49> buf2;
    utils::net::punycode::encode(u8"ウィキペディア", buf);
    assert(utils::strutil::equal(buf.buf, "cckbak0byl6e"));
    utils::net::punycode::decode(buf.buf, buf2);
    assert(utils::strutil::equal(buf2.buf, u8"ウィキペディア"));
    buf = {}, buf2 = {};
    utils::net::punycode::encode(u8"bücher", buf);
    assert(utils::strutil::equal(buf.buf, "bcher-kva"));
    utils::net::punycode::decode(buf.buf, buf2);
    assert(utils::strutil::equal(buf2.buf, u8"bücher"));
    buf = {}, buf2 = {};
    utils::net::punycode::encode_host(u8"Go言語.com", buf);
    assert(utils::strutil::equal(buf.buf, u8"xn--go-hh0g6u.com", utils::strutil::ignore_case()));
    utils::net::punycode::decode_host(buf.buf, buf2);
    assert(utils::strutil::equal(buf2.buf, u8"Go言語.com", utils::strutil::ignore_case()));
    buf = {}, buf2 = {};
    utils::net::punycode::encode_host(u8"太郎.com", buf);
    assert(utils::strutil::equal(buf.buf, "xn--sssu80k.com", utils::strutil::ignore_case()));
}

int main() {
    test_punycode();
}
