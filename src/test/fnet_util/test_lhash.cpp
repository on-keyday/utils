/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/util/sha1.h>
#include <fnet/util/base64.h>
#include <strutil/equal.h>
#include <helper/pushbacker.h>
#include <fnet/util/md5.h>

constexpr auto make_sample(auto sample) {
    utils::helper::FixedPushBacker<char[21], 21> result;
    utils::sha::make_sha1(sample, result);
    return result;
}

constexpr auto b64enc(auto enc) {
    utils::helper::FixedPushBacker<char[35], 34> encoded;
    utils::base64::encode(enc, encoded);
    return encoded;
}

void test_sha1() {
    constexpr auto sample = make_sample("The quick brown fox jumps over the lazy dog");
    constexpr auto encoded = b64enc(sample.buf);
    static_assert(utils::strutil::equal(encoded.buf, "L9ThxnotKPzthJ7hu3bnORuT6xI="));
    constexpr auto empty = make_sample("");
    constexpr auto encoded2 = b64enc(empty.buf);
    static_assert(utils::strutil::equal(encoded2.buf, "2jmj7l5rSw0yVb/vlWAYkK/YBwk="));
}

int main() {
    test_sha1();
    utils::md5::test::check_md5();
}
