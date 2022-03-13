/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <net/http2/request.h>
#include <net/async/tcp.h>
#include <net/async/pool.h>
#include <async/async_macro.h>
#include <net/ssl/ssl.h>

using namespace utils;

void test_http2protocol() {
    net::set_iocompletion_thread(true);
    net::start([](async::Context& ctx) {
        auto tcp = AWAIT(net::open_async("google.com", "https"));
        auto ssl = AWAIT(net::open_async(std::move(tcp), "./src/test/net/cacert.pem", "\2h2", "google.com"));
        auto h2 = AWAIT(net::http2::open_async(std::move(ssl)));
        auto setting = {std::pair{net::http2::SettingKey::enable_push, 0}};
        auto h2ctx = AWAIT(net::http2::negotiate(std::move(h2), setting));
        net::http::Header h;
        h.set(":method", "GET");
        h.set(":authority", "google.com");
        h.set(":path", "/");
        h.set(":scheme", "https");
        auto resp = std::move(AWAIT(net::http2::request(h2ctx, std::move(h))));
        auto rh = resp.response();
    }).wait();
}

int main(int, char**) {
    test_http2protocol();
}
