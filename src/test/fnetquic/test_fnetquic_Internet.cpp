/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/quic.h>
#include <env/env_sys.h>
#include <fnet/addrinfo.h>
#include <fnet/socket.h>
#include <fnet/connect.h>
using namespace futils::fnet::quic::use::rawptr;

using path = futils::wrap::path_string;
path libssl;
path libcrypto;
std::string cert;

void load_env() {
    auto env = futils::env::sys::env_getter();
    env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
    env.get_or(cert, "FNET_NET_CERT", "cert.pem");
}

int main() {
    load_env();
    futils::fnet::tls::set_libcrypto(libcrypto.data());
    futils::fnet::tls::set_libssl(libssl.data());
    auto ctx = std::make_shared<Context>();
    namespace tls = futils::fnet::tls;
    auto conf = tls::configure();
    assert(conf);
    conf.set_alpn("\x02h3");
    conf.set_cacert_file(cert.data());
    auto config = use_default_config(std::move(conf));
    auto ok = ctx->init(std::move(config)) &&
              ctx->connect_start("www.google.com");
    assert(ok);
    auto [sock, addr] = futils::fnet::connect("www.google.com", "443", futils::fnet::sockattr_udp(), false).value();

    while (true) {
        auto [data, _, idle] = ctx->create_udp_payload();
        if (!idle) {
            break;
        }
        if (data.size()) {
            sock.writeto(addr.addr, data);
        }
        futils::byte buf[2000];
        auto rdata = sock.readfrom(buf);
        if (rdata && rdata->first.size()) {
            ctx->parse_udp_payload(rdata->first);
        }
        if (ctx->handshake_confirmed()) {
            ctx->request_close(AppError{1, "sorry, this client doesn't implement http/3. currently, connectivity test."});
        }
    }
    auto alpn = ctx->get_selected_alpn();
    assert(alpn == "h3");
}
