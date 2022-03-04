/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <net/async/dns.h>
#include <net/tcp/tcp.h>
#include <net/async/tcp.h>
#include <net/async/pool.h>
#include <net/ssl/ssl.h>
#include <platform/windows/io_completetion_port.h>
#include <thread>
#include <wrap/cout.h>

void test_asynctcp2() {
    using namespace utils;
    auto fetch = [](const char* host, const char* path = "/") {
        return net::get_pool().start<wrap::string>([=](async::Context& ctx) {
            auto a = net::query(host, "https");
            a.wait_or_suspend(ctx);
            auto addr = a.get();
            auto c = net::async_open(std::move(addr));
            c.wait_or_suspend(ctx);
            auto cntcp = c.get();
            assert(cntcp);
            auto s = net::open_async(std::move(cntcp), "./src/test/net/cacert.pem");
            s.wait_or_suspend(ctx);
            auto conn = s.get();

            wrap::string http;
            http += "GET ";
            http += path;
            http += " HTTP/1.1\r\nHost: ";
            http += host;
            http += "\r\n\r\n";
            auto f = conn->write(http.c_str(), http.size());
            // assert(f.is_done());
            f.wait_or_suspend(ctx);
            wrap::string buf;
            buf.resize(2048);
            auto v = conn->read(buf.data(), buf.size());
            v.wait_or_suspend(ctx);
            auto red = v.get();
            buf.resize(red.read);
            ctx.set_value(buf);
        });
    };
    std::thread([] {
        auto iocp = platform::windows::get_iocp();
        while (true) {
            iocp->wait_callbacks(8, ~0);
        }
    }).detach();
    utils::wrap::cout_wrap() << fetch("syosetu.com").get() << "\ndone\n";
}

int main(int argc, char** argv) {
    test_asynctcp2();
}