/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/http3/stream.h>
#include <fnet/http3/typeconfig.h>
#include <fnet/quic/quic.h>
#include <fnet/util/qpack/typeconfig.h>
#include <env/env_sys.h>
#include <fnet/addrinfo.h>
#include <fnet/socket.h>
using namespace utils::fnet::quic::use::rawptr;

using path = utils::wrap::path_string;
path libssl;
path libcrypto;
std::string cert;

void load_env() {
    auto env = utils::env::sys::env_getter();
    env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
    env.get_or(cert, "FNET_NET_CERT", "cert.pem");
}

int main() {
    namespace http3 = utils::fnet::http3;
    namespace tls = utils::fnet::tls;
    using QpackConfig = utils::qpack::TypeConfig<>;
    using Conn = http3::stream::Connection<http3::TypeConfig<QpackConfig, DefaultTypeConfig>>;
    tls::set_libcrypto(libcrypto.data());
    tls::set_libssl(libssl.data());
    auto conf = tls::configure();
    conf.set_alpn("\x02h3");
    conf.set_cacert_file(cert.data());
    auto ctx = use_default_context<DefaultTypeConfig>(std::move(conf));
    assert(ctx);
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

    return 0;
}
