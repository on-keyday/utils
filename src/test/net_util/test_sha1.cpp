/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <net_util/sha1.h>
#include <net_util/base64.h>
#include <helper/equal.h>
#include <helper/pushbacker.h>

constexpr auto make_sample(auto sample) {
    utils::helper::FixedPushBacker<char[21], 21> result;
    utils::net::sha::make_sha1(sample, result);
    return result;
}

constexpr auto b64enc(auto enc) {
    utils::helper::FixedPushBacker<char[35], 34> encoded;
    utils::net::base64::encode(enc, encoded);
    return encoded;
}

void test_sha1() {
    constexpr auto sample = make_sample("The quick brown fox jumps over the lazy dog");
    constexpr auto encoded = b64enc(sample.buf);
    static_assert(utils::helper::equal(encoded.buf, "L9ThxnotKPzthJ7hu3bnORuT6xI="));
    constexpr auto empty = make_sample("");
    constexpr auto encoded2 = b64enc(empty.buf);
    static_assert(utils::helper::equal(encoded2.buf, "2jmj7l5rSw0yVb/vlWAYkK/YBwk="));
}

int main() {
    test_sha1();
}
