/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/server/server.h>
#include <fnet/quic/quic.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <thread>
#include <wrap/cout.h>
#include <env/env_sys.h>
namespace fnet = utils::fnet;
namespace quic = utils::fnet::quic;
using HandlerMap = quic::server::HandlerMap<quic::use::rawptr::DefaultTypeConfig>;

struct ServerContext {
    std::shared_ptr<HandlerMap> hm;
    fnet::Socket sock;
    std::atomic_uint ref;
    std::atomic_bool end;
};

void recv_packets(std::shared_ptr<ServerContext> ctx) {
    while (!ctx->end) {
        utils::byte data[2000];
        auto [payload, addr, err] = ctx->sock.readfrom(data);
        if (err) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        ctx->hm->parse_udp_payload(payload, addr);
    }
}

void schedule_events(std::shared_ptr<ServerContext> ctx) {
    while (!ctx->end) {
        ctx->hm->schedule();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void send_packets(std::shared_ptr<ServerContext> ctx) {
    fnet::flex_storage buffer;
    while (!ctx->end) {
        auto [data, addr, exist] = ctx->hm->create_packet(buffer);
        if (!exist) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (data.size()) {
            ctx->sock.writeto(addr, data);
        }
    }
}
using path = utils::wrap::path_string;
path libssl;
path libcrypto;

void load_env(std::string& cert, std::string& prv) {
    auto env = utils::env::sys::env_getter();
    env.get_or(libssl, "FNET_QUIC_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    env.get_or(libcrypto, "FNET_QUIC_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
    env.get_or(cert, "FNET_QUIC_LOCAL_CERT", "cert.pem");
    env.get_or(prv, "FNET_QUIC_LOCAL_PRIVATE_KEY", "private.pem");
    utils::fnet::tls::set_libcrypto(libcrypto.data());
    utils::fnet::tls::set_libssl(libssl.data());
}

int quic_server() {
    std::string cert, prv;
    load_env(cert, prv);
    auto& cout = utils::wrap::cout_wrap();
    auto ctx = std::make_shared<ServerContext>();
    ctx->hm = std::make_shared<HandlerMap>();
    auto tls_conf = utils::fnet::tls::configure();
    tls_conf.set_cert_chain(cert.c_str(), prv.c_str());
    utils::fnet::tls::ALPNCallback cb;
    cb.select = [](utils::fnet::tls::ALPNSelector& selector, void*) {
        utils::view::rvec name;
        while (selector.next(name)) {
            if (name == "test") {
                selector.select(name);
                return true;
            }
        }
        return false;
    };
    ctx->sock.set_DF(true);
    tls_conf.set_alpn_select_callback(&cb);
    // tls_conf.set_cert_chain();
    ctx->hm->set_config(utils::fnet::quic::use::use_default_config(std::move(tls_conf)));
    auto [wait, err] = fnet::get_self_server_address("8090", fnet::sockattr_udp(6));
    assert(!err);
    auto [list, err2] = wait.wait();
    fnet::Socket sock;
    while (list.next()) {
        auto addr = list.sockaddr();
        sock = fnet::make_socket(addr.attr);
        if (!sock) {
            continue;
        }
        err = sock.bind(addr.addr);
        if (err) {
            continue;
        }
        err = sock.set_ipv6only(false);
        if (err) {
            continue;
        }
        break;
    }
    assert(sock);
    ctx->sock = std::move(sock);
    std::thread(recv_packets, ctx).detach();
    std::thread(send_packets, ctx).detach();
    std::thread(schedule_events, ctx).detach();
    while (!ctx->end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    return 0;
}
