/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <net/http2/conn.h>
#include <net/async/pool.h>
#include <net/async/tcp.h>
#include <net/ssl/ssl.h>
#include <wrap/cout.h>
#include <async/async_macro.h>

void test_http2base() {
    using namespace utils;
    net::set_iocompletion_thread(true);
    net::get_pool()
        .start(
            [](async::Context& ctx) {
                auto f = net::open_async("google.com", "https");
                f.wait_until(ctx);
                auto tcp = f.get().conn;
                assert(tcp);
                auto s = net::open_async(std::move(tcp), "./src/test/net/cacert.pem", "\2h2", "google.com");
                s.wait_until(ctx);
                auto ssl = s.get();
                assert(ssl.conn);
                auto h2 = net::http2::open_async(std::move(ssl.conn));
                auto conn = AWAIT(h2);
                net::http2::SettingsFrame settings;
                settings.type = net::http2::FrameType::settings;
                settings.id = 0;
                settings.len = 0;
                AWAIT(conn->write(settings));
                auto frame = AWAIT(conn->read());
                assert(frame->type == net::http2::FrameType::settings);
                settings = {0};
                settings.type = net::http2::FrameType::settings;
                settings.flag = net::http2::ack;
                AWAIT(conn->write(settings));
                auto ackframe = AWAIT(conn->read());
                wrap::cout_wrap() << "done!\n";
            })
        .wait();
}

int main() {
    test_http2base();
}
