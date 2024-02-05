/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/util/base64.h>
#include <strutil/equal.h>
#include <helper/pushbacker.h>

constexpr auto test_encode(auto& in) {
    futils::helper::FixedPushBacker<char[13], 13> buf;
    futils::base64::encode(in, buf);
    return buf;
}

constexpr auto test_decode(auto& in) {
    futils::helper::FixedPushBacker<char[13], 13> buf;
    futils::base64::decode(in, buf);
    return buf;
}

constexpr auto be(auto input) {
    futils::binary::Buf<decltype(input)> buf;
    buf.write_be(input);
    return buf;
}

constexpr auto li(auto input) {
    futils::binary::Buf<decltype(input)> buf;
    buf.write_le(input);
    return buf;
}

void test_base64() {
    constexpr auto result = test_encode("hello");
    static_assert(futils::strutil::equal(result.buf, "aGVsbG8="));
    constexpr auto result2 = test_decode(result.buf);
    static_assert(futils::strutil::equal(result2.buf, "hello"));
    constexpr auto b = be(-0x03000000);
    constexpr auto l = li(0x03000000);
    static_assert(b.read_be() == -0x03000000);
    static_assert(l.read_le() == 0x03000000);
    static_assert(l.read_be() == 0x00000003);
}

int main() {
    test_base64();
}
