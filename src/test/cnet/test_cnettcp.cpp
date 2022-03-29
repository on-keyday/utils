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
    test::Timer timer;
    auto conn = cnet::tcp::create_client();
    cnet::set_callback(
        conn, [](cnet::CNet*, void*) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return false;
        },
        nullptr);
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
    assert(suc);
    cout << body;
}

int main() {
    test_tcp_cnet();
}
