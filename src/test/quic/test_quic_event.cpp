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

core::QueState server_proc(core::EventLoop*, core::QUIC* q, core::EventArg arg) {
    auto w = core::get_io(q);
    auto iow = io::udp::get_common_iowait(arg.prev);
    if (ok(iow->res)) {
        w->write_to(&iow->target, (const byte*)"hello client", 12);
    }
    auto t = iow->target;
    io::udp::del_iowait(q, arg.prev);
    auto ms = std::chrono::milliseconds(1);
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(ms);
    io::udp::enque_io_wait(
        q, t, w, 0, {dur.count()},
        core::Event{
            .arg = {.type = core::EventType::normal},
            .callback = server_proc,
        });
    return core::que_enque;
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
    auto ms = std::chrono::milliseconds(1000);
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(ms);
    io::udp::enque_io_wait(
        quic, t, proto, 0, {dur.count()},
        core::Event{
            .arg = {.type = core::EventType::normal},
            .callback = server_proc,
        });
    while (core::progress_loop(lc)) {
    }
    core::rem_loop(lc, quic);
}

int main() {
    test_quic_event();
}
