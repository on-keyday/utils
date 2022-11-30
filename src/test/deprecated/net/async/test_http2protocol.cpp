/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <deprecated/net/http2/request.h>
#include <deprecated/net/async/tcp.h>
#include <deprecated/net/async/pool.h>
#include <async/async_macro.h>
#include <deprecated/net/ssl/ssl.h>
#include <wrap/cout.h>

using namespace utils;

auto& cout = wrap::cout_wrap();

void test_http2protocol() {
    net::set_iocompletion_thread(true);
    auto spawn = [](const char* host, const char* path = "/") {
        return net::start(
            [](async::Context& ctx, const char* host, const char* path) {
                auto tcp = AWAIT(net::open_async(host, "https"));
                assert(tcp.conn);
                auto ssl = AWAIT(net::open_async(std::move(tcp.conn), "./src/test/net/cacert.pem", "\2h2", host));
                assert(ssl.conn);
                auto sel = ssl.conn->alpn_selected(nullptr);
                auto h2 = AWAIT(net::http2::open_async(std::move(ssl.conn)));
                auto setting = {std::pair{net::http2::SettingKey::enable_push, 0}};
                auto h2ctx = AWAIT(net::http2::negotiate(std::move(h2.conn), setting));
                net::http::Header h;
                h.set(":method", "GET");
                h.set(":authority", host);
                h.set(":path", path);
                h.set(":scheme", "https");
                auto resp = std::move(AWAIT(net::http2::request(h2ctx.ctx, std::move(h))));
                auto rh = resp.resp.response();
                cout << rh.response() << "body\n"
                     << rh.body();
            },
            host, path);
    };
    auto g = spawn("google.com");
    auto m = spawn("gmail.com");
    g.wait();
    m.wait();
}

int main(int, char**) {
    test_http2protocol();
}
