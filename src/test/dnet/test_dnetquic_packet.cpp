/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/quic/packet_wire/versionnego.h>
#include <dnet/quic/packet_number.h>
#include <dnet/quic/packet_wire/initial_packet.h>
#include <dnet/quic/packet_wire/handshake_0rtt.h>

int main() {
    using namespace utils::dnet::quic::packet::test;
    auto val = check_version_negotiation();
}
