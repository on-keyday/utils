/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/context/context.h>
#include <random>
#include <fnet/quic/version.h>
#include <chrono>
#include <fnet/socket.h>
#include <fnet/plthead.h>
#include <fnet/addrinfo.h>
#include <thread>
#include <fnet/quic/quic.h>
using namespace utils::fnet;

void test_fnetquic_context() {
    tls::set_libcrypto(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/crypto.dll"));
    tls::set_libssl(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/ssl.dll"));
    auto ctx = std::make_shared<quic::context::Context<quic::use::smartptr::DefaultTypeConfig>>();
    quic::context::Config config;
    config.tls_config = tls::configure();
    config.tls_config.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    config.tls_config.set_alpn("\x02h3");
    config.connid_parameters.random = {nullptr, [](std::shared_ptr<void>&, utils::view::wvec data) {
                                           std::random_device dev;
                                           std::uniform_int_distribution uni(0, 255);
                                           for (auto& d : data) {
                                               d = uni(dev);
                                           }
                                           return true;
                                       }};
    config.internal_parameters.clock = quic::time::Clock{nullptr, 1, [](void*) {
                                                             return quic::time::Time(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                                                         }};
    auto val = ctx->connect_start();
    assert(val);
    utils::view::rvec data;

    auto wait = resolve_address("localhost", "8090", {.socket_type = SOCK_DGRAM});
    assert(!wait.second);
    auto [info, err] = wait.first.wait();
    assert(!err);
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
        auto d = utils::view::wvec(const_cast<utils::byte*>(data.data()), data.size());
        ctx->parse_udp_payload(d);
        sock = std::move(insock);
    };
    auto get_sock = [](Socket& sock) -> decltype(auto) { return sock; };
    utils::byte buf[3000];
    while (true) {
        std::tie(data, val) = ctx->create_udp_payload();
        if (!val && ctx->is_closed()) {
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
    test_fnetquic_context();
}
