/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/util/ipaddr.h>
#include <array>
#include <number/array.h>

constexpr auto testv6(auto str) {
    auto seq = utils::make_ref_seq(str);
    std::array<std::uint8_t, 16> out;
    utils::ipaddr::parse_ipv6(seq, out);
    return out;
}

auto testv6n(auto str) {
    auto seq = utils::make_ref_seq(str);
    std::array<std::uint8_t, 16> out;
    utils::ipaddr::parse_ipv6(seq, out);
    return out;
}

constexpr auto testv4(auto str) {
    auto seq = utils::make_ref_seq(str);
    std::array<std::uint8_t, 4> out;
    utils::ipaddr::parse_ipv4(seq, out);
    return out;
}

constexpr auto outv6(auto str, bool v4 = false) {
    utils::number::Array<char, 50, true> data{};
    auto seq = utils::make_ref_seq(str);
    utils::ipaddr::ipv6_to_string(data, str, v4);
    return data;
}

int main() {
    using namespace utils;
    constexpr auto localhost = testv6("::1");
    constexpr auto zero = testv6("0000:0000:0000:0000:0000:0000:0000:0000");
    constexpr auto zero_omit = testv6("0::");
    constexpr auto zero_value = testv6("0::0:0:0:0:0:1");
    constexpr auto google_com = testv6("2404:6800:4004:811::200e");
    constexpr auto localhost_v4 = testv4("127.0.0.1");
    constexpr auto netmask_v4 = testv4("255.255.255.255");
    constexpr auto google_com_o = outv6(google_com);
    constexpr auto localhost_o = outv6(localhost);
    constexpr auto ipv4mapped = testv6("::ffff:127.0.0.1");
    constexpr auto ipv4mapped_o = outv6(ipv4mapped, true);
    auto obj = google_com_o;
    auto obj2 = localhost_o;
}
