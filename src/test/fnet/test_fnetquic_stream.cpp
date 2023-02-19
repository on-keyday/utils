/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/stream/impl/conn.h>
#include <mutex>
#include <testutil/alloc_hook.h>
#include <fnet/debug.h>
#include <cassert>
#include <fnet/quic/frame/parse.h>

namespace test {
    namespace stream = utils::fnet::quic::stream;
    namespace impl = utils::fnet::quic::stream::impl;

    stream::InitialLimits gen_local_limits() {
        stream::InitialLimits limits;
        limits.bidi_stream_data_local_limit = 1000;
        limits.bidi_stream_data_remote_limit = 1000;
        limits.bidi_stream_limit = 100;
        limits.uni_stream_data_limit = 500;
        limits.uni_stream_limit = 200;
        limits.conn_data_limit = 10000;
        return limits;
    }

    stream::InitialLimits gen_peer_limits() {
        stream::InitialLimits limits;
        limits.bidi_stream_data_local_limit = 900;
        limits.bidi_stream_data_remote_limit = 900;
        limits.bidi_stream_limit = 600;
        limits.uni_stream_data_limit = 700;
        limits.uni_stream_limit = 300;
        limits.conn_data_limit = 9000;
        return limits;
    }

}  // namespace test

struct RecvQ {
    utils::fnet::slib::list<std::shared_ptr<test::impl::BidiStream<std::mutex>>> que;

    auto accept() {
        auto res = std::move(que.front());
        que.pop_front();
        return res;
    }
};

int main() {
    int count = 0;
    auto pers = [&](int i, auto) {
        if (count == 20) {
            ;
        }
        count++;
        return 1;
    };
    utils::fnet::set_normal_allocs(utils::fnet::debug::allocs(&pers));
    utils::fnet::set_objpool_allocs(utils::fnet::debug::allocs(&pers));
    utils::test::set_alloc_hook(true);
    using IOResult = utils::fnet::quic::IOResult;
    auto peer = test::gen_peer_limits();
    auto local = test::gen_local_limits();
    auto conn_local = test::impl::make_conn<std::mutex>(test::stream::Direction::client);
    conn_local->apply_initial_limits(local, peer);
    auto conn_peer = test::impl::make_conn<std::mutex>(test::stream::Direction::server);
    conn_peer->apply_initial_limits(peer, local);
    auto stream_1 = conn_local->open_bidi();
    assert(stream_1 && stream_1->sender.id() == 2);
    stream_1->sender.add_data("Hello peer!", false);
    utils::byte traffic[1000];
    utils::io::writer w{traffic};
    std::vector<std::weak_ptr<utils::fnet::quic::ack::ACKLostRecord>> locals, peers;
    namespace frame = utils::fnet::quic::frame;
    frame::fwriter fw{w};
    IOResult ok = conn_local->send(fw, locals);
    assert(ok == IOResult::ok && locals.size() == 1);
    RecvQ q;
    conn_peer->set_accept_bidi(std::shared_ptr<RecvQ>(&q, [](RecvQ*) {}), [](std::shared_ptr<void>& v, std::shared_ptr<test::impl::BidiStream<std::mutex>> d) {
        static_cast<RecvQ*>(v.get())->que.push_back(std::move(d));
    });

    utils::io::reader r{w.written()};
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto [res, e] = conn_peer->recv(f);
        assert(res && e.is_noerr());
        auto stream_1_peer = q.accept();
        assert(stream_1_peer && stream_1_peer->receiver.id() == 2);
        res = stream_1_peer->receiver.request_stop_sending(23);
        assert(res);
        w.reset();
        ok = conn_peer->send(fw, peers);
        assert(ok == IOResult::ok);
        return true;
    });
    locals[0].lock()->ack();
    auto res = stream_1->sender.detect_ack_lost();
    assert(res == IOResult::no_data);
    r.reset(w.written());
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto [res, e] = conn_local->recv(f);
        assert(res && e.is_noerr());
        w.reset();
        auto ok = conn_local->send(fw, locals);
        assert(ok == IOResult::ok);
    });
    res = stream_1->sender.detect_ack_lost();
    assert(res == IOResult::no_data);
    r.reset(w.written());
    peers[0].lock()->ack();
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto [res, e] = conn_peer->recv(f);
        assert(res && e.is_noerr());
    });
    locals[1].lock()->ack();
    res = stream_1->sender.detect_ack_lost();
    assert(res == IOResult::not_in_io_state);
}
