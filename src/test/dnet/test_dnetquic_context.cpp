/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/quic/quic.h>
#include <dnet/addrinfo.h>
#include <dnet/socket.h>
#include <dnet/plthead.h>
#include <thread>
using namespace utils::dnet;

void test_dnetquic_context() {
    set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
    auto conf = quic::default_config();
    auto tls = create_tls();
    tls.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    auto q = quic::make_quic(tls, std::move(conf));
    q.tls().set_alpn("\x02h3", 3);
    q.tls().set_hostname("localhost");
    auto err = q.connect();
    auto str = err.error<std::string>();
    assert(err == error::block);
    byte data[3000]{};
    size_t red = 0;
    auto res = q.receive_udp_datagram(data, 3000, red);
    assert(res);
    SockAddr addr;
    Socket sock;
    auto waddr = resolve_address("localhost", "8090", {.address_family = AF_INET, .socket_type = SOCK_DGRAM, .protocol = 0});
    auto reslv = waddr.wait();
    while (reslv.next()) {
        addr = reslv.sockaddr();
        sock = make_socket(addr.attr.address_family, addr.attr.socket_type, addr.attr.protocol);
        if (!sock) {
            continue;
        }
        break;
    }
    // sock.set_connreset(false);
    assert(sock);
    sock.writeto(addr.addr, data, red);
    using namespace std::chrono_literals;
    while (true) {
        auto [n, peer, err] = sock.readfrom(data);
        if (err) {
            if (isSysBlock(err)) {
                std::this_thread::sleep_for(1ms);
                continue;
            }
            assert(!err);
        }
        str = peer.to_string<std::string>();
        q.provide_udp_datagram(data, n.size());
        break;
    }
    err = q.connect();
    str = err.error<std::string>();
    assert(!err);
}

int main() {
    test_dnetquic_context();
}
