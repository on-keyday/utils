/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/packet/wire.h>
#include <fnet/quic/packet/crypto.h>
#include <fnet/quic/packet/creation.h>

int main() {
    using namespace futils::fnet::quic::packet;
    auto val = test::check_onertt();
    val = test::check_render_in_place();
    val = test::check_packet_creation();
}
