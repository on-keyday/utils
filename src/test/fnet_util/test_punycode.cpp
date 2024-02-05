/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/util/punycode.h>
#include <helper/pushbacker.h>
#include <strutil/equal.h>

void test_punycode() {
    futils::helper::FixedPushBacker<char[50], 49> buf;
    futils::helper::FixedPushBacker<char8_t[50], 49> buf2;
    futils::net::punycode::encode(u8"ウィキペディア", buf);
    assert(futils::strutil::equal(buf.buf, "cckbak0byl6e"));
    futils::net::punycode::decode(buf.buf, buf2);
    assert(futils::strutil::equal(buf2.buf, u8"ウィキペディア"));
    buf = {}, buf2 = {};
    futils::net::punycode::encode(u8"bücher", buf);
    assert(futils::strutil::equal(buf.buf, "bcher-kva"));
    futils::net::punycode::decode(buf.buf, buf2);
    assert(futils::strutil::equal(buf2.buf, u8"bücher"));
    buf = {}, buf2 = {};
    futils::net::punycode::encode_host(u8"Go言語.com", buf);
    assert(futils::strutil::equal(buf.buf, u8"xn--go-hh0g6u.com", futils::strutil::ignore_case()));
    futils::net::punycode::decode_host(buf.buf, buf2);
    assert(futils::strutil::equal(buf2.buf, u8"Go言語.com", futils::strutil::ignore_case()));
    buf = {}, buf2 = {};
    futils::net::punycode::encode_host(u8"太郎.com", buf);
    assert(futils::strutil::equal(buf.buf, "xn--sssu80k.com", futils::strutil::ignore_case()));
}

int main() {
    test_punycode();
}
