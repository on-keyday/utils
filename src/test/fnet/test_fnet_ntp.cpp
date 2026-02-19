/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/ntp.h>
#include <fnet/connect.h>
#include <random>
namespace fnet = futils::fnet;

int main() {
    auto [sock, addr] = fnet::connect("ntp.nict.jp", "123", fnet::sockattr_udp(), false).value();

    fnet::ntp::Packet pkt;
    pkt.set_version(4);
    pkt.set_mode(fnet::ntp::Mode::client);
    pkt.set_leap(fnet::ntp::LeapWarning::none);
    pkt.precision = 0x20;
    std::random_device dev;
    std::uniform_int_distribution<std::uint64_t> d;
    pkt.transmit_timestamp = d(dev);
}
