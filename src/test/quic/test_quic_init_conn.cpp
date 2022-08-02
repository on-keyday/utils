/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/flow/init.h>
#include <quic/crypto/crypto.h>
#include <quic/io/udp.h>
#include <quic/io/async.h>
using namespace utils::quic;

void test_quic_init_conn() {
    auto q = core::default_QUIC();
    flow::InitialConfig config;
    config.host = "localhost";
    config.alpn = "\2h3";
    config.alpn_len = 3;
    config.cert = "";
    config.connID = (const byte*)"client hel";
    config.connIDlen = 10;
    config.token = (const byte*)"custom_token";
    config.tokenlen = 12;
    tpparam::DefinedParams params = tpparam::defaults;
    params.max_udp_payload_size = 1500;
    auto err = flow::start_conn_client(q, config, [&](conn::Connection* c, const byte* data, tsize len) {
        assert(data);
        auto io = core::get_io(q);
        io::Target t;
        t = io::IPv4(127, 0, 0, 1, 8089);
        t.key = io::udp::UDP_IPv4;
        auto res = io->new_target(&t, client);
        assert(ok(res));
        t.target = io::target_id(res);
        res = io->write_to(&t, data, len);
        assert(ok(res));
        res = io::register_target(q, t.target);
        assert(ok(res));
        io::udp::enque_io_wait(
            q, t, io, 0, {1000},
            io::udp::make_iocb(core::get_alloc(q), [=](io::Target* t, const byte* data, tsize len, io::Result res) {
                assert(ok(res));
            }));
    });
    assert(err == flow::Error::none);
    auto ev = core::new_Looper(q);
    while (core::progress_loop(ev)) {
    }
}

int main() {
    constexpr auto bssl_libssl = "D:/quictls/boringssl/built/lib/ssl.dll";
    constexpr auto bssl_libcrypto = "D:/quictls/boringssl/built/lib/crypto.dll";
    external::set_libcrypto_location(bssl_libcrypto);
    external::set_libssl_location(bssl_libssl);
    auto okc = external::load_LibCrypto();
    auto oks = external::load_LibSSL();
    assert(okc && oks);
    test_quic_init_conn();
}
