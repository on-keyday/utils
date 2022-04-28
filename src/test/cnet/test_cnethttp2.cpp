/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/http2.h>
#include <cnet/tcp.h>
#include <cnet/ssl.h>
#include <map>
#include <string>
#include <number/parse.h>
#include <testutil/timer.h>
#include <wrap/cout.h>
auto& cout = utils::wrap::cout_wrap();

void test_cnet_http2() {
    using namespace utils::cnet;
    namespace h2 = utils::net::http2;
    utils::test::Timer start;
    auto tcp = tcp::create_client();
    auto host = "www.google.com";
    auto path = "/";
    tcp::set_hostport(tcp, host, "https");
    bool ok = open(tcp);
    assert(ok);
    auto tls = ssl::create_client();
    ssl::set_certificate_file(tls, "src/test/net/cacert.pem");
    ssl::set_alpn(tls, "\2h2");
    ssl::set_host(tls, host);
    set_lowlevel_protocol(tls, tcp);
    ok = open(tls);
    auto code = get_error(tls);
    assert(ok);
    auto alpn = ssl::get_alpn_selected(tls);
    assert(strncmp(alpn, "h2", 2) == 0);
    cout << start.next_step() << "\n";
    auto client = http2::create_client();
    std::int32_t id = 0;
    size_t data_size_sum = 0;
    http2::set_frame_callback(client, [&](http2::Frames* fr) {
        bool res = true;
        for (auto& frame : fr) {
            http2::default_proc(fr, frame, http2::DefaultProc::all);
            if (http2::is_settings_ack(frame)) {
                std::map<std::string, std::string> h;
                http2::set_request(h, host, path);
                http2::write_header(fr, h, id);
            }
            if (id != 0 && frame->id == id) {
                if (frame->type == h2::FrameType::header) {
                    std::multimap<std::string, std::string> h;
                    http2::read_header(fr, h, frame->id);
                    bool no_length = true;
                    for (auto& field : h) {
                        if (field.first == "content-length") {
                            size_t incr = 0;
                            utils::number::parse_integer(field.second, incr);
                            http2::write_window_update(fr, incr, 0);
                            http2::write_window_update(fr, incr, frame->id);
                            no_length = false;
                        }
                    }
                    if (no_length) {
                        http2::write_window_update(fr, 0xffffff, 0);
                        http2::write_window_update(fr, 0xffffff, id);
                    }
                }
                std::string data;
                if (frame->type == h2::FrameType::data) {
                    http2::read_data(fr, data, frame->id);
                    data_size_sum += frame->len;
                }
                if (http2::is_close_stream_signal(frame)) {
                    res = false;
                }
            }
        }
        return res;
    });
    set_lowlevel_protocol(client, tls);
    ok = open(client);
    assert(ok);
    ok = http2::poll_frame(client);
    code = get_error(tcp);
    assert(ok);
    cout << start.delta();
    delete_cnet(client);
}

int main() {
    test_cnet_http2();
}
