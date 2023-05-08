/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <coro/coro.h>
#include <fnet/quic/quic.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <mutex>
#if __has_include(<format>)
#include <format>
#endif
#include <thread>
using namespace utils::fnet::quic::use;
struct Recvs {
    std::shared_ptr<Reader> r;
    std::shared_ptr<RecvStream> s;
};

struct ThreadData {
    std::shared_ptr<Context> ctx;
    int req_count = 0;
    int done_count = 0;
    int total_req = 0;
    size_t transmit = 0;
};

void recv_stream(utils::coro::C* c, Recvs* s) {
    auto th = static_cast<ThreadData*>(c->get_common_context());
    std::vector<utils::fnet::flex_storage> read;
    const auto d = utils::helper::defer([&] {
        delete s;
        th->req_count--;
        th->done_count++;
        for (auto& r : read) {
            th->transmit += r.size();
        }
        if (th->total_req == th->done_count) {
            c->add_coroutine<Context>(th->ctx.get(), [](utils::coro::C* c, Context* ctx) {
                ctx->request_close(utils::fnet::quic::QUICError{
                    .msg = "request done",
                    .transport_error = utils::fnet::quic::TransportError::NO_ERROR,
                });
                while (!ctx->is_closed()) {
                    c->suspend();
                }
                c->add_coroutine(nullptr, [](utils::coro::C* c, void*) {
                    auto th = static_cast<ThreadData*>(c->get_common_context());
                    c->suspend();
                });
            });
        }
    });
    while (!s->s->is_closed()) {
        auto [data, eof] = s->r->read_direct();
        if (data.size()) {
            read.push_back(std::move(data));
        }
        if (eof) {
            break;
        }
        s->s->update_recv_limit([](utils::fnet::quic::stream::Limiter lim) {
            if (lim.avail_size() < 5000) {
                return lim.curlimit() + 10000;
            }
            return lim.curlimit();
        });
        c->suspend();
    }
}

void request(utils::coro::C* c, void* p) {
    auto n = std::uintptr_t(p);
    auto th = static_cast<ThreadData*>(c->get_common_context());
    auto streams = th->ctx->get_streams();
    while (true) {
        auto stream = streams->open_uni();
        if (!stream) {
            c->suspend();
            continue;
        }
        auto id = stream->id();
#if __has_include(<format>)
        stream->add_data(std::format("GET /search?q=Q{} HTTP/1.1\r\nHost: www.google.com\r\n", n), false);
#else
        std::string data;
        data = "GET /search?q=Q" + utils::number::to_string<std::string>(n) + " HTTP/1.1\r\nHost: www.google.com\r\n";
#endif
        // stream->add_data("Accept-Encoding: gzip\r\n", false, true);
        c->suspend();
        assert(id == stream->id());
        stream->add_data("\r\n", true, true);
        c->suspend();
        assert(id == stream->id());
        stream->add_data({}, true, true);
        break;
    }
    th->req_count++;
    th->total_req++;
    c->add_coroutine(nullptr, [](utils::coro::C* c, void*) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });
}

void conn(utils::coro::C* c, std::shared_ptr<Context>& ctx, utils::fnet::Socket& sock, utils::fnet::NetAddrPort& addr) {
    auto s = ctx->get_streams();
    // s->set_auto_remove(true);
    s->set_accept_uni(std::shared_ptr<utils::coro::C>(c, [](auto) {}), [](std::shared_ptr<void>& c, std::shared_ptr<RecvStream> in) {
        auto s = set_stream_reader(*in);
        auto l = static_cast<utils::coro::C*>(c.get());
        auto r = new Recvs();
        r->r = std::move(s);
        r->s = in;
        if (!l->add_coroutine(r, recv_stream)) {
            delete r;
            in->request_stop_sending(1);
        }
    });
    while (true) {
        auto [payload, idle] = ctx->create_udp_payload();
        if (!idle) {
            break;
        }
        if (payload.size()) {
            sock.writeto(addr, payload);
        }
        utils::byte data[1500];
        auto [recv, addr, err] = sock.readfrom(data);
        if (recv.size()) {
            ctx->parse_udp_payload(recv);
        }
        s->update_max_data([](utils::fnet::quic::stream::Limiter v) {
            if (v.avail_size() < 5000) {
                return v.curlimit() + 10000;
            }
            return v.curlimit();
        });
    }
}

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
    conf.set_alpn("\x04test");
    conf.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    ctx->init(use_default(std::move(conf)));
    auto streams = ctx->get_streams();
    ctx->connect_start();
    std::atomic<utils::coro::C*> ld = nullptr;
    std::thread([&, ctx] {
        auto c = utils::coro::make_coro(10, 300);
        ThreadData data;
        data.ctx = ctx;
        data.req_count = 0;
        c.set_common_context(&data);
        ld.store(&c);
        while (!ctx->is_closed()) {
            c.run();
        }
    }).detach();
    while (!ld.load()) {
    }
    auto c = ld.load();
    std::thread([c] {
        for (std::uintptr_t i = 0; i < 100; i++) {
            c->add_coroutine((void*)(i), request);
        }
    }).detach();
    conn(c, ctx, sock, to);
}
