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
#include <net/http/http1.h>
#include <net/http/header.h>
#include <thread>
#include <wrap/cout.h>

void test_asynctcp2() {
    using namespace utils;
    auto fetch = [](const char* host, const char* path = "/") {
        return net::get_pool().start<wrap::string>([=](async::Context& ctx) {
            auto c = net::open_async(host, "https");
            c.wait_or_suspend(ctx);
            auto cntcp = c.get();
            assert(cntcp);
            auto s = net::open_async(std::move(cntcp), "./src/test/net/cacert.pem");
            s.wait_or_suspend(ctx);
            auto conn = s.get();
            auto h = net::request_async(std::move(conn), host, "GET", path, {});
            h.wait_or_suspend(ctx);
            auto resp = std::move(h.get());
            auto header = resp.response();
            auto res = wrap::string(header.response());
            // res += header.body();
            return ctx.set_value(res);
            /*
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
            ctx.set_value(buf);*/
        });
    };
    std::thread([] {
        auto iocp = platform::windows::get_iocp();
        while (true) {
            iocp->wait_callbacks(8, ~0);
        }
    }).detach();
    auto& cout = utils::wrap::cout_wrap();
    auto s = fetch("syosetu.com");
    auto g = fetch("www.google.com");
    auto m = fetch("docs.microsoft.com");
    cout << s.get() << "\ndone\n";
    cout << g.get() << "\ndone\n";
    cout << m.get() << "\ndone\n";
}

int main(int argc, char** argv) {
    test_asynctcp2();
}
