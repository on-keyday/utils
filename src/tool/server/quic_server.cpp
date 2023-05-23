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
        ctx->hm->parse_udp_payload(payload);
    }
}

void send_packets(std::shared_ptr<ServerContext> ctx) {
    fnet::flex_storage buffer;
    while (!ctx->end) {
        auto [data, exist] = ctx->hm->create_packet(buffer);
        if (!exist) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (data.size()) {
            // ctx->sock.writeto(, data);
        }
    }
}

int quic_server() {
    auto ctx = std::make_shared<ServerContext>();
    ctx->hm = std::make_shared<HandlerMap>();
    auto [wait, err] = fnet::get_self_server_address("8090", fnet::sockattr_udp());
    assert(err);
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
        break;
    }
    assert(sock);
    ctx->sock = std::move(sock);
    std::thread(recv_packets, ctx).detach();
    std::thread(send_packets, ctx).detach();
    while (!ctx->end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    return 0;
}
