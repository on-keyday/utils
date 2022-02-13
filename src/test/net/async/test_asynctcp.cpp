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
    auto& pool = net::get_pool();
    pool.set_yield(true);
    auto spawn = [&](const char* host) {
        return pool.start([host](async::Context& ctx) mutable {
            auto pack = utils::wrap::pack();
            auto co = net::async_open(host, "http");
            assert(co.state() != async::TaskState::invalid);
            auto begin = system_clock::now();
            co.wait_or_suspend(ctx);
            auto end = system_clock::now();
            auto time = [&] {
                return duration_cast<milliseconds>(end - begin);
            };
            pack << "query:\n"
                 << time() << "\n";
            begin = system_clock::now();
            auto conn = co.get();
            assert(conn);
            wrap::string text = "GET / HTTP/1.1\r\nHost: ";
            text += host;
            text += "\r\n\r\n";
            auto st = conn->write(text.c_str(), text.size());
            while (st == net::State::running) {
                ctx.suspend();
                st = conn->write(text.c_str(), text.size());
            }
            wrap::string data;
            st = net::read(data, *conn);
            while (st == net::State::running) {
                ctx.suspend();
                st = net::read(data, *conn);
            }
            end = system_clock::now();
            pack << "data:\n"
                 << data << "\n";
            pack << "time:\n"
                 << time() << "\n";
            utils::wrap::cout_wrap() << std::move(pack);
        });
    };
    auto v = spawn("google.com");
    auto u = spawn("httpbin.org");
    auto begin = system_clock::now();
    v.wait();
    u.wait();
    auto end = system_clock::now();
    utils::wrap::cout_wrap() << "\ntotal time\n"
                             << duration_cast<milliseconds>(end - begin) << "\n";
}

int main() {
    test_asynctcp();
}
