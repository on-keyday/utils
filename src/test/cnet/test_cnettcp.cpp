/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/tcp.h>
#include <cassert>
#include <cstring>
#include <thread>
#include <wrap/light/string.h>
#include <wrap/cout.h>
#include <net/http/http_headers.h>
#include <wrap/light/hash_map.h>
#include <number/array.h>
#include <testutil/timer.h>
using namespace utils;
auto& cout = wrap::cout_wrap();

void test_tcp_cnet() {
    test::Timer local_timer;
    auto conn = cnet::tcp::create_client();
    auto cb = [&](cnet::CNet* ctx) {
        if (!cnet::protocol_is(ctx, "tcp")) {
            return true;
        }
        if (cnet::tcp::is_waiting(ctx)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return false;
        }
        auto p = cnet::tcp::get_current_state(ctx);
        if (p == cnet::tcp::TCPStatus::start_resolving_name) {
            cout << "start timer\n";
            local_timer.reset();
        }
        else if (p == cnet::tcp::TCPStatus::resolve_name_done) {
            cout << "dns resolving:" << local_timer.delta() << "\n";
        }
        else if (p == cnet::tcp::TCPStatus::connected) {
            cout << "tcp connecting:" << local_timer.delta() << "\n";
        }
        return true;
    };
    cnet::set_lambda(conn, cb);
    cnet::tcp::set_hostport(conn, "www.google.com", "http");
    auto suc = cnet::open(conn);
    assert(suc);
    auto text = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
    size_t w = 0;
    cnet::write(conn, text, ::strlen(text), &w);
    wrap::string buf, body;
    wrap::hash_multimap<wrap::string, wrap::string> h;
    suc = net::h1header::read_response<wrap::string>(buf, helper::nop, h, body, [&](auto& seq, size_t expect, bool finalcall) {
        number::Array<1024, char> tmpbuf{0};
        size_t r = 0;
        while (!r) {
            suc = cnet::read(conn, tmpbuf.buf, tmpbuf.capacity(), &r);
            assert(suc);
        }
        seq.buf.buffer.append(tmpbuf.buf, r);
        return true;
    });
    if (!suc) {
        cout << "error:" << cnet::get_error(conn) << "\n";
        return;
    }

    auto d = local_timer.delta();
    // cout << body << "\n";
    cout << "http responding:" << d << "\n";
    cnet::delete_cnet(conn);
}

int main() {
    test_tcp_cnet();
}
