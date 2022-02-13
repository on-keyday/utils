/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <net/async/tcp.h>
#include <net/async/pool.h>
#include <wrap/cout.h>
#include <chrono>

void test_asynctcp() {
    using namespace utils;
    using namespace std::chrono;
    auto co = net::async_open("google.com", "http");
    auto begin = system_clock::now();
    co.wait();
    auto end = system_clock::now();
    auto time = duration_cast<milliseconds>(end - begin);
    utils::wrap::cout_wrap() << time << "\n";
    auto conn = co.get();
    if (!conn) {
        utils::wrap::cout_wrap() << "failed\n";
        return;
    }
    auto v = net::get_pool().start([=](async::Context& ctx) mutable {
        auto text = "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n";
        while (conn->write(text, strlen(text)) == net::State::running) {
            ctx.suspend();
        }
    });
    v.wait();
}

int main() {
    test_asynctcp();
}
