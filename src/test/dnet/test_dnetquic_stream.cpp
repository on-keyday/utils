/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/quic/stream/impl/conn_manage.h>
#include <mutex>
#include <testutil/alloc_hook.h>
#include <dnet/debug.h>
#include <cassert>
#include <dnet/quic/frame/parse.h>

namespace test {
    namespace stream = utils::dnet::quic::stream;
    namespace impl = utils::dnet::quic::stream::impl;

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

int main() {
    auto pers = [](int i) {
        if (i) {
            return 0;
        }
        return 1;
    };
    utils::dnet::set_normal_allocs(utils::dnet::debug::allocs(&pers));
    utils::dnet::set_objpool_allocs(utils::dnet::debug::allocs(&pers));
    utils::test::set_alloc_hook(true);
    using IOResult = utils::dnet::quic::IOResult;
    auto peer = test::gen_peer_limits();
    auto local = test::gen_local_limits();
    auto conn_local = test::impl::make_conn<std::mutex>(test::stream::Direction::client);
    conn_local->apply_initial_limits(local, peer);
    auto conn_peer = test::impl::make_conn<std::mutex>(test::stream::Direction::server);
    conn_peer->apply_initial_limits(peer, local);
    auto stream_1 = conn_local->open_bidi();
    assert(stream_1 && stream_1->send.id() == 2);
    stream_1->send.add_data("Hello peer!", false);
    utils::byte traffic[1000];
    utils::io::writer w{traffic};
    std::vector<std::weak_ptr<utils::dnet::quic::ack::ACKLostRecord>> locals, peers;
    namespace frame = utils::dnet::quic::frame;
    frame::fwriter fw{w};
    auto ok = stream_1->send.send(0, fw, locals);
    assert(ok == IOResult::ok && locals.size() == 1);

    utils::io::reader r{w.written()};
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto stream = frame::cast<frame::StreamFrame>(&f);
        assert(stream);
        auto e = conn_peer->check_creation(f.type.type_detail(), stream->streamID.value);
        assert(e.is_noerr());
        auto stream_1_peer = conn_peer->accept_bidi();
        assert(stream_1_peer && stream_1_peer->recv.id() == 2);
        e = stream_1_peer->recv.recv(0, *stream);
        assert(e.is_noerr());
        auto res = stream_1_peer->recv.request_stop_sending(23);
        assert(res);
        w.reset();
        ok = stream_1_peer->recv.send_stop_sending(fw, peers);
        assert(ok == IOResult::ok);
        return true;
    });
    locals[0].lock()->ack();
    auto state = stream_1->send.detect_ack();
    assert(state == test::stream::SendState::send);
    r.reset(w.written());
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto stop = frame::cast<frame::StopSendingFrame>(&f);
        assert(stop);
        auto e = conn_local->check_creation(f.type.type_detail(), stop->streamID.value);
        assert(e.is_noerr());
        stream_1->send.recv_stop_sending(*stop);
        w.reset();
        ok = stream_1->send.send_reset(fw, locals);
        assert(ok == IOResult::ok);
    });
    state = stream_1->send.detect_ack();
    assert(state == test::stream::SendState::reset_sent);
    r.reset(w.written());
    peers[0].lock()->ack();
    frame::parse_frame<std::vector>(r, [&](frame::Frame& f, bool err) {
        assert(!err);
        auto reset = frame::cast<frame::ResetStreamFrame>(&f);
        assert(reset);
        auto e = conn_peer->check_creation(f.type.type_detail(), reset->streamID.value);
        assert(e.is_noerr());
        auto stream_1_peer = conn_peer->find_recv_bidi(reset->streamID.value);
        e = stream_1_peer->recv.recv_reset(*reset);
        assert(e.is_noerr());
    });
    locals[1].lock()->ack();
    state = stream_1->send.detect_ack();
    assert(state == test::stream::SendState::reset_recved);
}
