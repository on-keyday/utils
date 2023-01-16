/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/quic/context/context.h>
#include <random>
#include <dnet/quic/version.h>
#include <chrono>
#include <dnet/socket.h>
#include <dnet/plthead.h>
#include <dnet/addrinfo.h>
using namespace utils::dnet;

void test_dnetquic_context() {
    set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
    auto ctx = std::make_shared<quic::context::Context<std::mutex>>();
    ctx->crypto.tls = create_tls();
    ctx->crypto.tls.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    ctx->init_tls();
    ctx->crypto.tls.set_alpn("\2h3", 3);
    ctx->connIDs.issuer.user_gen_random = [](std::shared_ptr<void>&, utils::view::wvec data) {
        std::random_device dev;
        std::uniform_int_distribution uni(0, 255);
        for (auto& d : data) {
            d = uni(dev);
        }
        return true;
    };
    ctx->conf.version = quic::version_1;
    ctx->ackh.clock.now_fn = [](void*) {
        return quic::time::Time(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    };
    ctx->ackh.clock.granularity = 1;
    auto val = ctx->connect_start();
    assert(val);
    utils::view::rvec data;

    auto wait = resolve_address("localhost", "8090", {.socket_type = SOCK_DGRAM});
    auto info = wait.wait();
    assert(!wait.failed());
    Socket sock;
    NetAddrPort addr;
    while (info.next()) {
        auto saddr = info.sockaddr();
        sock = make_socket(saddr.attr);
        if (!sock) {
            continue;
        }
        addr = saddr.addr;
        break;
    }
    auto fn = [&](Socket& insock, utils::view::rvec data, NetAddrPort addr, bool, error::Error err) {
        assert(!err);
        auto d = utils::view::wvec(const_cast<byte*>(data.data()), data.size());
        ctx->parse_udp_payload(d);
        sock = std::move(insock);
    };
    auto get_sock = [](Socket& sock) -> decltype(auto) { return sock; };
    byte buf[3000];
    while (true) {
        std::tie(data, val) = ctx->create_udp_payload();
        if (!val && ctx->conn_err == utils::dnet::quic::ack::errIdleTimeout) {
            break;
        }
        assert(val);
        if (data.size()) {
            sock.writeto(addr, data);
        }
        while (true) {
            auto [d, peer, err] = sock.readfrom(buf);
            if (isSysBlock(err)) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                break;
            }
            ctx->parse_udp_payload(d);
        }
    }
}

int main() {
    test_dnetquic_context();
}
