/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net_util/sha1.h"
#include "../../include/net_util/base64.h"
#include "../../include/helper/equal.h"
#include "../../include/helper/pushbacker.h"

void test_sha1() {
    utils::helper::FixedPushBacker<char[21], 21> result;
    utils::helper::FixedPushBacker<char[35], 34> encoded;
    utils::net::sha::make_sha1("The quick brown fox jumps over the lazy dog", result);
    utils::net::base64::encode(result.buf, encoded);
    assert(utils::helper::equal(encoded.buf, "L9ThxnotKPzthJ7hu3bnORuT6xI="));
    result = {};
    encoded = {};
    utils::net::sha::make_sha1("", result);
    utils::net::base64::encode(result.buf, encoded);
    assert(utils::helper::equal(encoded.buf, "2jmj7l5rSw0yVb/vlWAYkK/YBwk="));
}

int main() {
    test_sha1();
}
