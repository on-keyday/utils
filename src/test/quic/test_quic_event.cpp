/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/core/core.h>
#include <quic/io/udp.h>
#include <quic/io/async.h>
#include <cassert>
#include <chrono>

using namespace utils::quic;

void server_proc(io::IO* w, io::Target* t, io::Result res) {
    if (ok(res)) {
        w->write_to(t, (const byte*)"hello client", 12);
    }
}

void enque_job(core::QUIC* q, io::Target m, io::IO* w) {
    auto ms = std::chrono::milliseconds(1000);
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(ms);
    io::udp::enque_io_wait(
        q, m, w, 0, {dur.count()},
        io::udp::make_iocb(core::get_alloc(q), [=](io::Target* t, const byte* d, tsize len, io::Result res) {
            server_proc(w, t, res);
            enque_job(q, m, w);
        }));
}

void test_quic_event() {
    auto quic = core::new_QUIC(allocate::stdalloc());
    core::set_bpool(quic, bytes::stdbpool(quic));
    auto lc = core::new_Looper(quic);
    core::register_io(quic, io::udp::Protocol(core::get_alloc(quic)));
    core::add_loop(lc, quic);
    io::IO* proto = core::get_io(quic);
    auto t = io::IPv4(127, 0, 0, 1, 8080);
    t.key = io::udp::UDP_IPv4;
    auto res = proto->new_target(&t, server);
    assert(ok(res));
    t.target = target_id(res);
    res = io::register_target(quic, t.target);
    assert(ok(res));
    enque_job(quic, t, proto);
    while (core::progress_loop(lc)) {
    }
    core::rem_loop(lc, quic);
}

int main() {
    test_quic_event();
}
