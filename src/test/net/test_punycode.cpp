/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/punycode.h"
#include "../../include/helper/pushbacker.h"
#include "../../include/helper/strutil.h"

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
}

int main() {
    test_punycode();
}
