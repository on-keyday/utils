/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/quic.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
using namespace utils::fnet::quic::use::rawptr;
namespace tls = utils::fnet::tls;
namespace quic = utils::fnet::quic;

int main() {
#ifdef _WIN32
    utils::fnet::tls::set_libcrypto(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/crypto.dll"));
    utils::fnet::tls::set_libssl(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/ssl.dll"));
#endif
    auto [wait, err] = utils::fnet::resolve_address("localhost", "8090", utils::fnet::sockattr_udp());
    assert(!err);
    auto [info, err2] = wait.wait();
    assert(!err2);
    utils::fnet::Socket sock;
    utils::fnet::NetAddrPort to;
    while (info.next()) {
        auto addr = info.sockaddr();
        sock = utils::fnet::make_socket(addr.attr);
        if (!sock) {
            continue;
        }
        to = std::move(addr.addr);
        sock.connect(to);
        break;
    }
    assert(sock);
    auto ctx = std::make_shared<Context>();
    auto conf = utils::fnet::tls::configure();
    conf.set_eraly_data_enabled(true);
    conf.set_alpn("\x04test");
    conf.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    utils::fnet::tls::Session sess;
    conf.set_session_callback([](utils::fnet::tls::Session&& sess, void* cb) {
        *(utils::fnet::tls::Session*)cb = sess;
        return true;
    },
                              &sess);
    auto def = use_default(std::move(conf));
    bool reqested = false;
    utils::fnet::flex_storage s;
    auto set_zero_rtt_obj = [&] {
        def.zero_rtt.obj = std::shared_ptr<void>(&s, [](auto) {});
    };
    set_zero_rtt_obj();
    def.zero_rtt.store_token = [](void* p, utils::view::rvec key, quic::token::Token tok) {
        auto& st = *((utils::fnet::flex_storage*)p);
        st = tok.token;
    };
    def.zero_rtt.find_token = [](void* p, utils::view::rvec key) {
        auto& st = *((utils::fnet::flex_storage*)p);
        if (st.size() != 0) {
            return quic::token::TokenStorage{utils::fnet::make_storage(st.rvec())};
        }
        return quic::token::TokenStorage{};
    };
    ctx->init(std::move(def));
    ctx->connect_start("localhost");
    while (true) {
        auto [payload, _, idle] = ctx->create_udp_payload();
        if (!idle) {
            break;
        }
        if (payload.size()) {
            sock.writeto(to, payload);
        }
        utils::byte data[1500];
        auto [recv, addr, err] = sock.readfrom(data);
        if (recv.size()) {
            ctx->parse_udp_payload(recv);
        }
        if (s.size() && !reqested) {
            ctx->request_close(AppError{.msg = utils::fnet::error::Error("OK")});
        }
    }
    set_zero_rtt_obj();
    def.session = sess;
    auto res = ctx->init(std::move(def));
    ctx->connect_start("localhost");
    auto streams = ctx->get_streams();
    auto stream = streams->open_uni();
    assert(stream);
    stream->add_data("GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n", true);
    while (true) {
        auto [payload, _, idle] = ctx->create_udp_payload();
        if (!idle) {
            break;
        }
        if (payload.size()) {
            sock.writeto(to, payload);
        }
        utils::byte data[1500];
        auto [recv, addr, err] = sock.readfrom(data);
        if (recv.size()) {
            ctx->parse_udp_payload(recv);
        }
    }
}
