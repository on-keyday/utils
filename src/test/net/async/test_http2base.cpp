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

void test_http2base() {
    using namespace utils;
    net::set_iocompletion_thread(true);
    net::get_pool()
        .start(
            [](async::Context& ctx) {
                auto f = net::open_async("google.com", "https");
                f.wait_or_suspend(ctx);
                auto s = net::open_async(std::move(f.get()), "./src/test/net/cacert.pem");
                s.wait_or_suspend(ctx);
                auto ssl = s.get();
                auto h2 = net::http2::open_async(std::move(ssl));
                h2.wait_or_suspend(ctx);
                auto conn = h2.get();
                net::http2::SettingsFrame settings;
                settings.type = net::http2::FrameType::settings;
                settings.id = 0;
                settings.len = 0;
                conn->write(settings).wait_or_suspend(ctx);
                auto setting_frame = conn->read();
                auto frame = setting_frame.wait_or_suspend(ctx).get();
                assert(frame->type == net::http2::FrameType::settings);
            })
        .wait();
}

int main() {
    test_http2base();
}