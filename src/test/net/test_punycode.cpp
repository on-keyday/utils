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
    utils::helper::FixedPushBacker<char[100], 99> buf;
    utils::net::punycode::encode("ウィキペディア", buf);
    assert(utils::helper::equal(buf.buf, "xn--cckbak0byl6e"));
}

int main() {
    test_punycode();
}