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
using namespace utils::dnet;

void test_dnetquic_context() {
    set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
    auto conf = quic::default_config();
    auto tls = create_tls();
    tls.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    auto q = quic::make_quic(tls, conf);
    q.tls().set_alpn("\x02h3", 3);
    q.connect();
    assert(q.block());
    byte data[3000]{};
    size_t red = 0;
    auto res = q.receive_quic_packets(data, 3000, &red);
    assert(res);
    SockAddr addr;
    addr.hostname = "localhost";
    addr.namelen = strlen(addr.hostname);
    addr.af = AF_INET;
    addr.type = SOCK_DGRAM;
    addr.proto = 0;
    Socket sock;
    auto waddr = resolve_address(addr, "8090");
    auto reslv = waddr.wait();
    while (reslv.next()) {
        reslv.sockaddr(addr);
        sock = make_socket(addr.af, addr.type, addr.proto);
        if (!sock) {
            continue;
        }
    }
    assert(sock);
    sock.writeto(addr.addr, addr.addrlen, data, red);
}

int main() {
    test_dnetquic_context();
}
