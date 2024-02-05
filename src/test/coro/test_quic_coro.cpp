/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <coro/coro.h>
#include <fnet/quic/quic.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <mutex>
#ifdef _WIN32
#include <format>
#define HAS_FORMAT
#endif
#include <thread>
#include <fnet/quic/server/server.h>
#include <wrap/cout.h>
#include <fnet/quic/ack/unacked.h>
#include <fnet/debug.h>
#include <testutil/alloc_hook.h>
#include <env/env_sys.h>
#include <fnet/connect.h>
using namespace futils::fnet::quic::use::rawptr;
namespace quic = futils::fnet::quic;
using TConfig2 = quic::context::TypeConfig<std::mutex, DefaultStreamTypeConfig, quic::connid::TypeConfig<>, quic::status::NewReno, quic::ack::UnackedPackets>;
// using ContextT = quic::context::Context<TConfig2>;
using ContextT = Context;
struct Recvs {
    std::shared_ptr<Reader> r;
    std::shared_ptr<RecvStream> s;
};

struct ThreadData {
    std::shared_ptr<ContextT> ctx;
    int req_count = 0;
    int done_count = 0;
    int total_req = 0;
    size_t transmit = 0;
};

void recv_stream(futils::coro::C* c, Recvs* s) {
    auto th = static_cast<ThreadData*>(c->get_thread_context());
    std::vector<futils::fnet::flex_storage> read;
    const auto d = futils::helper::defer([&] {
        delete s;
        th->req_count--;
        th->done_count++;
        for (auto& r : read) {
            th->transmit += r.size();
        }
        if (th->total_req == th->done_count) {
            c->add_coroutine<ContextT>(th->ctx.get(), [](futils::coro::C* c, ContextT* ctx) {
                ctx->request_close(futils::fnet::quic::QUICError{
                    .msg = "request done",
                    .transport_error = futils::fnet::quic::TransportError::NO_ERROR,
                });
                while (!ctx->is_closed()) {
                    c->suspend();
                }
                c->add_coroutine(nullptr, [](futils::coro::C* c, void*) {
                    // auto th = static_cast<ThreadData*>(c->get_common_context());
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
        s->s->update_recv_limit([&](FlowLimiter lim, std::uint64_t) {
            if (lim.avail_size() < 5000) {
                futils::wrap::cout_wrap() << "update max stream data seq: " << s->s->id().seq_count() << "\n";
                return lim.current_limit() + 100000;
            }
            return lim.current_limit();
        });
        c->suspend();
    }
}

void request(futils::coro::C* c, void* p) {
    auto n = std::uintptr_t(p);
    auto th = static_cast<ThreadData*>(c->get_thread_context());
    auto streams = th->ctx->get_streams();
    while (true) {
        auto stream = streams->open_uni();
        if (!stream) {
            c->suspend();
            continue;
        }
        auto id = stream->id();
#ifdef HAS_FORMAT
        stream->add_data(std::format("GET /search?q={} HTTP/1.1\r\nHost: google.com\r\n", n), false);
#else
        std::string data;
        data = "GET /search?q=Q" + futils::number::to_string<std::string>(n) + " HTTP/1.1\r\nHost: www.google.com\r\n";
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
    futils::wrap::cout_wrap() << "request sent " << n << "\n";
    th->req_count++;
    th->total_req++;
    c->add_coroutine(nullptr, [](futils::coro::C* c, void*) {
        futils::wrap::cout_wrap() << "sleeping\n";
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
}

void conn(futils::coro::C* c, std::shared_ptr<ContextT>& ctx, futils::fnet::Socket& sock, futils::fnet::NetAddrPort& addr) {
    auto s = ctx->get_streams();
    // s->set_auto_remove(true);
    auto ch = s->get_conn_handler();
    ch->arg = c;
    ch->uni_accept_cb = [](void*& c, std::shared_ptr<RecvStream> in) {
        auto s = set_stream_reader(*in);
        auto l = static_cast<futils::coro::C*>(c);
        auto r = new Recvs();
        r->r = std::move(s);
        r->s = in;
        if (!l->add_coroutine(r, recv_stream)) {
            delete r;
            in->request_stop_sending(1);
        }
    };
    /*
    s->set_accept_uni(c, [](void*& c, std::shared_ptr<RecvStream> in) {
        auto s = set_stream_reader(*in);
        auto l = static_cast<futils::coro::C*>(c);
        auto r = new Recvs();
        r->r = std::move(s);
        r->s = in;
        if (!l->add_coroutine(r, recv_stream)) {
            delete r;
            in->request_stop_sending(1);
        }
    });
    */
    while (true) {
        auto [payload, _, idle] = ctx->create_udp_payload();
        if (!idle) {
            break;
        }
        if (payload.size()) {
            sock.writeto(addr, payload);
        }
        futils::byte data[1500];
        auto recv = sock.readfrom(data);
        if (recv && recv->first.size()) {
            ctx->parse_udp_payload(recv->first);
        }
        s->update_max_data([&](FlowLimiter v, std::uint64_t) {
            if (v.avail_size() < 5000) {
                futils::wrap::cout_wrap() << "update max data\n";
                return v.current_limit() + 100000;
            }
            return v.current_limit();
        });
    }
}

using path = futils::wrap::path_string;
path libssl;
path libcrypto;
std::string cert;

void load_env() {
    auto env = futils::env::sys::env_getter();
    env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
    env.get_or(cert, "FNET_PUBLIC_KEY", "cert.pem");
}

int main() {
    load_env();
    // futils::test::set_alloc_hook(true);
    // futils::test::set_log_file("./ignore/memuse.log");
    // futils::fnet::debug::allocs();
#ifdef _WIN32
    futils::fnet::tls::set_libcrypto(libcrypto.data());
    futils::fnet::tls::set_libssl(libssl.data());
#endif
    auto [sock, to] = futils::fnet::connect("localhost", "8090", futils::fnet::sockattr_udp(), false).value();
    auto ctx = std::make_shared<ContextT>();
    auto conf = futils::fnet::tls::configure();
    conf.set_alpn("\x04test");
    conf.set_cacert_file(cert.data());
    auto def = use_default_config(std::move(conf));
    def.internal_parameters.use_ack_delay = true;
    ctx->init(std::move(def));
    auto streams = ctx->get_streams();
    ctx->connect_start();
    std::atomic<futils::coro::C*> ld = nullptr;
    std::thread([&, ctx] {
        auto c = futils::coro::make_coro(10, 300);
        ThreadData data;
        data.ctx = ctx;
        data.req_count = 0;
        c.set_thread_context(&data);
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
    conn(c, ctx, sock, to.addr);
}
