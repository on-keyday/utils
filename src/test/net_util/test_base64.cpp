/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <net_util/base64.h>
#include <helper/equal.h>
#include <helper/pushbacker.h>

constexpr auto test_encode(auto& in) {
    utils::helper::FixedPushBacker<char[13], 13> buf;
    utils::net::base64::encode(in, buf);
    return buf;
}

constexpr auto test_decode(auto& in) {
    utils::helper::FixedPushBacker<char[13], 13> buf;
    utils::net::base64::decode(in, buf);
    return buf;
}

constexpr auto be(auto input) {
    utils::endian::Buf<decltype(input)> buf;
    buf.write_be(input);
    return buf;
}

constexpr auto li(auto input) {
    utils::endian::Buf<decltype(input)> buf;
    buf.write_le(input);
    return buf;
}

void test_base64() {
    using namespace utils::net;
    constexpr auto result = test_encode("hello");
    static_assert(utils::helper::equal(result.buf, "aGVsbG8="));
    constexpr auto result2 = test_decode(result.buf);
    static_assert(utils::helper::equal(result2.buf, "hello"));
    constexpr auto b = be(-0x03000000);
    constexpr auto l = li(0x03000000);
    static_assert(b.read_be() == -0x03000000);
    static_assert(l.read_le() == 0x03000000);
    static_assert(l.read_be() == 0x00000003);
}

int main() {
    test_base64();
}
