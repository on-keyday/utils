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
    auto& cout = utils::wrap::cout_wrap();
    auto fetch = [&](const char* host, const char* path = "/") {
        return net::get_pool().start<wrap::string>([=, &cout](async::Context& ctx) {
            auto c = net::open_async(host, "https");
            c.wait_or_suspend(ctx);
            auto cntcp = c.get();
            assert(cntcp);
            auto s = net::open_async(std::move(cntcp), "./src/test/net/cacert.pem");
            s.wait_or_suspend(ctx);
            auto conn = s.get();
            net::Header header;
            header.set("User-Agent", "fetch");
            auto h = net::request_async(std::move(conn), host, "GET", path, std::move(header));
            h.wait_or_suspend(ctx);
            auto resp = std::move(h.get());
            auto response = resp.response();
            auto res = wrap::string(response.response());
            if (auto loc = response.value("Location")) {
                cout << "Redirect To: " << loc << "\n";
            }
            return ctx.set_value(res);
        });
    };
    std::thread([] {
        auto iocp = platform::windows::get_iocp();
        while (true) {
            iocp->wait_callbacks(8, ~0);
        }
    }).detach();

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
