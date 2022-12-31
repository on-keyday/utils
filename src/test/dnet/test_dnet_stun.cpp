/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/stun.h>
#include <dnet/socket.h>
#include <dnet/addrinfo.h>
#include <dnet/plthead.h>
#include <string>
#include <thread>

void test_dnet_stun_run(utils::dnet::WPacket& w, int af) {
    using namespace utils::dnet;
    using namespace std::chrono_literals;
    stun::StunContext ctx;
    SockAddr resolv;
    constexpr auto stun_sever = "stun.l.google.com";
    auto wait = resolve_address(stun_sever, "19302", {.address_family = af, .socket_type = SOCK_DGRAM});
    auto addr = wait.wait();
    assert(!wait.failed());
    Socket sock;
    SockAddr server;
    while (addr.next()) {
        sock = make_socket(af, SOCK_DGRAM, 0);
        assert(sock);
        server = addr.sockaddr();
        auto err = sock.connect(server.addr);
        if (!err || isSysBlock(err)) {
            break;
        }
    }
    assert(sock);
    auto [local, err] = sock.get_localaddr();
    assert(!err);
    auto str = local.to_string<std::string>();
    ctx.original_address.family = local.addr.type() == NetAddrType::ipv6 ? stun::family_ipv6 : stun::family_ipv4;
    ctx.original_address.port = local.port;
    memcpy(ctx.original_address.address, local.addr.data(), local.addr.size());
    byte duf[12]{1, 2, 3, 4, 5, 6, 4, 4, 4, 3, 0x93, 0x84};
    memcpy(ctx.transaction_id, duf, 12);
    stun::StunResult result = stun::StunResult::do_request;
    while (true) {
        if (result == stun::StunResult::do_request) {
            w.offset = 0;
            result = ctx.request(w);
            continue;
        }
        if (result == stun::StunResult::do_roundtrip) {
            sock.writeto(server.addr, utils::view::rvec(w.b.data, w.offset));
            int count = 0;
            while (true) {
                auto [n, addr, err] = sock.readfrom(utils::view::wvec(w.b.data, w.b.len));
                if (err) {
                    if (count > 100) {
                        result = ctx.no_response();
                        break;
                    }
                    if (isSysBlock(err)) {
                        std::this_thread::sleep_for(1ms);
                        count++;
                    }
                    continue;
                }
                auto recv = w.b.resized(n.size());
                result = ctx.response(recv);
                break;
            }
            continue;
        }
        break;
    }
}

void test_dnet_stun() {
    byte buf[2500];
    utils::dnet::WPacket w{{buf, sizeof(buf)}};
    test_dnet_stun_run(w, AF_INET);
}

int main() {
    test_dnet_stun();
}
