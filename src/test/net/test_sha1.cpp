/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/sha1.h"
#include "../../include/net/util/base64.h"
#include "../../include/helper/strutil.h"

void test_sha1() {
    utils::helper::FixedPushBacker<char[20], 20> result;
    utils::helper::FixedPushBacker<char[35], 34> encoded;
    utils::net::sha1::make_hash("The quick brown fox jumps over the lazy dog", result);
    utils::net::base64::encode(result.buf, encoded);
    assert(utils::helper::equal(encoded.buf, "L9ThxnotKPzthJ7hu3bnORuT6xI="));
}

int main() {
    test_sha1();
}