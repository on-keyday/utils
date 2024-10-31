/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../foreign.h"
#include <fnet/quic/server/server.h>
#include <fnet/quic/quic.h>
#include <fnet/connect.h>
#include <fnet/http3/http3.h>
#include <fnet/http3/mux.h>
#include <wrap/light/string.h>
#include <unicode/utf/convert.h>
#include <thread/concurrent_queue.h>
#include <fnet/util/uri.h>
#include <thread>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

using QuicContext = futils::fnet::quic::use::smartptr::Context;
using Multiplexer = futils::fnet::quic::server::HandlerMap<futils::fnet::quic::use::smartptr::DefaultTypeConfig>;

struct HttpRequest {
    futils::uri::URI<std::string> uri;
    std::shared_ptr<FuncDataAsync> callback;
};

struct Connections {
    std::shared_ptr<Multiplexer> connections;
    futils::fnet::quic::context::Config config;
    futils::fnet::Socket sock;
    std::shared_ptr<futils::thread::MultiProducerChannelBuffer<HttpRequest, std::allocator<HttpRequest>>> que;
};

static Connections connections;

static void schedule_recv() {
    while (connections.connections) {
        connections.connections->schedule_recv();
        std::this_thread::yield();
    }
}

static void schedule_send() {
    while (connections.connections) {
        connections.connections->schedule_send();
        std::this_thread::yield();
    }
}

static void do_send() {
    futils::byte buffer[65536];
    while (connections.connections) {
        auto [packet, addr, exist] = connections.connections->create_packet(futils::view::wvec(buffer));
        if (!exist) {
            std::this_thread::yield();
            continue;
        }
        auto res = connections.sock.writeto(addr, packet);
        if (!res) {
            std::this_thread::yield();
        }
    }
}
using Mgr = futils::fnet::BufferManager<futils::byte[65536]>;
static void recv_callback(futils::fnet::Socket&& sock, Mgr& mgr, futils::fnet::NetAddrPort addr_raw, futils::fnet::NotifyResult&& r) {
    auto expected = r.readfrom_unwrap(mgr.get_buffer(), addr_raw, [&](futils::view::wvec buf) {
        return sock.readfrom(buf);
    });
    const auto d = futils::helper::defer([&] {
        sock.readfrom_async(futils::fnet::async_addr_then(Mgr{}, recv_callback));
    });
    if (!expected) {
        return;
    }
    auto& [data, addr] = expected.value();
    connections.connections->parse_udp_payload(data, addr);
    sock.readfrom_until_block(mgr.get_buffer(), [&](futils::view::wvec data, futils::fnet::NetAddrPort addr) {
        connections.connections->parse_udp_payload(data, addr);
    });
}

static void do_recv() {
    connections.sock.readfrom_async(futils::fnet::async_addr_then(Mgr{}, recv_callback));
    while (connections.connections) {
        futils::fnet::wait_io_event(1);
    }
}

static bool init_connections(const char* cert, const char* key) {
    if (connections.connections) {
        return false;
    }
    auto tls_conf = futils::fnet::tls::configure_with_error();
    if (!tls_conf) {
        return false;
    }
    if (auto ok = tls_conf->set_cert_chain(cert, key); !ok) {
        return false;
    }
    auto sock = futils::fnet::make_socket(futils::fnet::sockattr_udp());
    if (!sock) {
        return false;
    }
    connections.sock = std::move(*sock);
    tls_conf->set_alpn("\x02h3");
    connections.config = futils::fnet::quic::use::use_default_config(std::move(*tls_conf));
    connections.connections = std::make_shared<Multiplexer>();
    auto& c = connections.connections;
    c->set_client_config({
        .init_recv_stream = [](std::shared_ptr<void>& app, futils::fnet::quic::use::smartptr::RecvStream& stream) { futils::fnet::quic::stream::impl::set_stream_reader(stream, true, Multiplexer::stream_recv_notify_callback); },
        .init_send_stream = [](std::shared_ptr<void>& app, futils::fnet::quic::use::smartptr::SendStream& stream) { futils::fnet::quic::stream::impl::set_on_send_callback(stream, Multiplexer::stream_send_notify_callback); },
        .on_transport_parameter_read = futils::fnet::http3::multiplexer_on_transport_parameter_read<futils::qpack::TypeConfig<futils::fnet::flex_storage>, futils::fnet::quic::use::smartptr::DefaultTypeConfig>,
        .on_uni_stream_recv = futils::fnet::http3::multiplexer_recv_uni_stream<futils::qpack::TypeConfig<futils::fnet::flex_storage>, futils::fnet::quic::use::smartptr::DefaultTypeConfig>,

    });
    std::thread(schedule_recv).detach();
    std::thread(schedule_send).detach();
    std::thread(do_send).detach();
    std::thread(do_recv).detach();
    return true;
}

extern "C" EXPORT void init_quic_client(ForeignCallback* cb) {
    std::string cert;
    std::string key;
    auto cert_arg = cb->get_arg(cb->data, 0);
    auto key_arg = cb->get_arg(cb->data, 1);
    auto libssl_path = cb->get_arg(cb->data, 2);     // optional
    auto libcrypto_path = cb->get_arg(cb->data, 3);  // optional
    auto ret = cb->get_return_value_location(cb->data);
    if (!cert_arg || !key_arg) {
        cb->set_boolean(ret, 0);
        return;
    }
    auto set = [](const char* value, size_t size, void* user) {
        auto str = (std::string*)user;
        str->append(value, size);
    };
    if (!cb->get_string_callback(cert_arg, &cert, set) ||
        !cb->get_string_callback(key_arg, &key, set)) {
        cb->set_boolean(ret, 0);
        return;
    }
    futils::wrap::path_string libssl;
    futils::wrap::path_string libcrypto;
    if (libssl_path && libcrypto_path) {
        std::string ssl;
        std::string crypto;
        if (!cb->get_string_callback(libssl_path, &ssl, set) ||
            !cb->get_string_callback(libcrypto_path, &crypto, set)) {
            cb->set_boolean(ret, 0);
            return;
        }
        libssl = futils::utf::convert<futils::wrap::path_string>(ssl);
        libcrypto = futils::utf::convert<futils::wrap::path_string>(crypto);
        futils::fnet::tls::set_libcrypto(libcrypto.c_str());
        futils::fnet::tls::set_libssl(libssl.c_str());
    }
    cb->set_boolean(ret, init_connections(cert.c_str(), key.c_str()));
}

extern "C" EXPORT void http_start_request(ForeignCallback* cb) {
    if (!connections.connections) {
        return;
    }
    auto arg = cb->get_arg(cb->data, 1);
    if (!arg) {
        return;
    }
    auto f = cb->get_func(arg);
    if (!f) {
        return;
    }
    auto async_f = cb->get_async_func(cb->data, f);
    if (!async_f) {
        return;
    }
    std::shared_ptr<FuncDataAsync> async_f_holder(async_f, [](auto* p) { p->unload(p); });
}

extern "C" EXPORT void hwrpg_foreign_test(ForeignCallback* cb) {
    auto phase = cb->get_callback_phase(cb->data);
    switch (phase) {
        case 0:
        case 1: {
            auto arg = cb->get_arg(cb->data, 0);
            if (!arg) {
                return;
            }
            auto f = cb->get_func(arg);
            if (!f) {
                return;
            }
            cb->call(cb->data, f);
            break;
        }
        default:
            break;
    }
}