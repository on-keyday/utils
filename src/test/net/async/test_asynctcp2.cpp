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
#include <testutil/timer.h>

#include <chrono>
#include <wrap/lite/vector.h>
#include <thread/lite_lock.h>

void test_asynctcp2() {
    using namespace utils;
    auto& cout = utils::wrap::cout_wrap();
    auto& pool = net::get_pool();
    pool.set_maxthread(1);
    std::atomic_size_t totaldelta;
    thread::LiteLock lock;
    wrap::vector<std::pair<const char*, std::chrono::milliseconds>> result;
    auto fetch = [&](const char* host, const char* path = "/") {
        return pool.start<wrap::string>([=, &cout, &totaldelta, &lock, &result](async::Context& ctx) {
            test::Timer timer;
            auto c = net::open_async(host, "https");
            c.wait_until(ctx);
            auto cntcp = c.get();
            assert(cntcp);
            auto s = net::open_async(std::move(cntcp), "./src/test/net/cacert.pem");
            s.wait_until(ctx);
            auto conn = s.get();
            net::Header header;
            header.set("User-Agent", "fetch");
            auto h = net::request_async(std::move(conn), host, "GET", path, std::move(header));
            h.wait_until(ctx);
            auto resp = std::move(h.get());
            auto response = resp.response();
            auto res = wrap::string(response.response());
            if (auto loc = response.value("Location")) {
                cout << "Redirect To: " << loc << "\n";
            }
            cout << res << "done\n";
            auto delt = timer.delta();
            totaldelta += delt.count();
            lock.lock();
            result.push_back({host, delt});
            lock.unlock();
        });
    };
    std::thread([] {
        auto iocp = platform::windows::get_iocp();
        while (true) {
            iocp->wait_callbacks(8, ~0);
        }
    }).detach();
    using namespace std::chrono;
    test::Timer timer;
    auto s = fetch("syosetu.com");
    auto g = fetch("www.google.com");
    auto m = fetch("docs.microsoft.com");
    auto d = fetch("httpbin.org", "/get");
    auto n = fetch("stackoverflow.com");
    auto e = fetch("example.com");
    s.wait();
    g.wait();
    m.wait();
    d.wait();
    n.wait();
    e.wait();
    cout << "real\n";
    cout << timer.delta().count() << "ms";
    cout << "\ntotal\n"
         << totaldelta << "ms\n";
    for (auto& p : result) {
        cout << "host " << p.first << "\n";
        cout << "time " << p.second.count() << "ms\n";
    }
}

int main(int argc, char** argv) {
    test_asynctcp2();
}
