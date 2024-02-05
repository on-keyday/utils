/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/stream/impl/conn.h>
#include <mutex>
#include <testutil/alloc_hook.h>
#include <fnet/debug.h>
#include <cassert>
#include <fnet/quic/frame/parse.h>
#include <fnet/quic/stream/impl/recv_stream.h>
#include <fnet/quic/stream/typeconfig.h>

namespace test {
    namespace stream = futils::fnet::quic::stream;
    namespace impl = futils::fnet::quic::stream::impl;
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
    futils::fnet::slib::list<std::shared_ptr<test::impl::BidiStream<test::stream::TypeConfig<std::mutex>>>> que;

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
    futils::fnet::set_normal_allocs(futils::fnet::debug::allocs(&pers));
    futils::fnet::set_objpool_allocs(futils::fnet::debug::allocs(&pers));
    futils::test::set_alloc_hook(true);
    using IOResult = futils::fnet::quic::IOResult;
    auto peer = test::gen_peer_limits();
    auto local = test::gen_local_limits();
    auto conn_local = test::impl::make_conn<test::stream::TypeConfig<std::mutex>>(test::stream::Origin::client);
    conn_local->apply_peer_initial_limits(peer);
    conn_local->apply_local_initial_limits(local);
    auto conn_peer = test::impl::make_conn<test::stream::TypeConfig<std::mutex>>(test::stream::Origin::server);
    conn_peer->apply_local_initial_limits(peer);
    conn_peer->apply_peer_initial_limits(local);
    auto stream_1 = conn_local->open_bidi();
    assert(stream_1 && stream_1->sender.id() == 0);
    stream_1->sender.add_data("Hello peer!", false);
    futils::byte traffic[1000];
    futils::binary::writer w{traffic};
    // std::vector<std::weak_ptr<futils::fnet::quic::ack::ACKLostRecord>> locals, peers;
    namespace frame = futils::fnet::quic::frame;
    futils::fnet::quic::ack::ACKRecorder locals, peers;
    frame::fwriter fw{w};
    IOResult ok = conn_local->send(fw, locals);
    // first, this program is not multi thread so use_count is correct,
    // second, conn_local, locals and return value here holds same shared_ptr
    // so, use_count() here should be 3
    assert(ok == IOResult::ok && locals.get().use_count() == 3);
    RecvQ q;
    auto h = conn_peer->get_conn_handler();
    h->arg = std::shared_ptr<RecvQ>(&q, [](RecvQ*) {});
    h->bidi_accept_cb = [](std::shared_ptr<void>& v, std::shared_ptr<test::impl::BidiStream<test::stream::TypeConfig<std::mutex>>> d) {
        auto recv = std::make_shared<futils::fnet::quic::stream::impl::RecvSorter<std::mutex>>();
        using H = futils::fnet::quic::stream::impl::FragmentSaver<>;
        d->receiver.set_receiver(H(std::move(recv),
                                   futils::fnet::quic::stream::impl::reader_recv_handler<std::mutex, test::stream::TypeConfig<std::mutex>>));
        static_cast<RecvQ*>(v.get())->que.push_back(std::move(d));
    };
    /*
    conn_peer->set_accept_bidi(std::shared_ptr<RecvQ>(&q, [](RecvQ*) {}), [](std::shared_ptr<void>& v, std::shared_ptr<test::impl::BidiStream<test::stream::TypeConfig<std::mutex>>> d) {
        auto recv = std::make_shared<futils::fnet::quic::stream::impl::RecvSorted<std::mutex>>();
        using H = futils::fnet::quic::stream::impl::FragmentSaver<>;
        d->receiver.set_receiver(H(std::move(recv),
                                   futils::fnet::quic::stream::impl::reader_recv_handler<std::mutex, test::stream::TypeConfig<std::mutex>>));
        static_cast<RecvQ*>(v.get())->que.push_back(std::move(d));
    });
    */

    futils::binary::reader r{w.written()};
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto [res, e] = conn_peer->recv(f);
        assert(res && e.has_no_error());
        auto stream_1_peer = q.accept();
        assert(stream_1_peer && stream_1_peer->receiver.id() == 0);
        res = stream_1_peer->receiver.request_stop_sending(23);
        assert(res);
        w.reset();
        ok = conn_peer->send(fw, peers);
        assert(ok == IOResult::ok);
        return true;
    });
    // locals[0].lock()->ack();
    locals.get()->ack();
    locals = {};  // clear this
    auto res = stream_1->sender.detect_ack_lost();
    assert(res == IOResult::no_data);
    r.reset(w.written());
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto [res, e] = conn_local->recv(f);
        assert(res && e.has_no_error());
        w.reset();
        auto ok = conn_local->send(fw, locals);
        assert(ok == IOResult::ok);
    });
    res = stream_1->sender.detect_ack_lost();
    assert(res == IOResult::no_data);
    r.reset(w.written());
    peers.get()->ack();
    peers = {};  // clear this
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto [res, e] = conn_peer->recv(f);
        assert(res && e.has_no_error());
    });
    locals.get()->ack();
    res = stream_1->sender.detect_ack_lost();
    assert(res == IOResult::not_in_io_state);
}
