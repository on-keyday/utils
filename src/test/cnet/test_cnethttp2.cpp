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
namespace h2 = utils::net::http2;

void print_frame(const char* on, const h2::Frame* frame) {
    cout << "\n";
    cout << on;
    cout << " frame\n";
    cout << "type: " << h2::frame_name(frame->type) << "\n";
    cout << "id: " << frame->id << "\n";
    cout << "len: " << frame->len << "\n";
    cout << "flag: " << h2::flag_state<std::string>(frame->flag, frame->type == h2::FrameType::ping || frame->type == h2::FrameType::settings)
         << "\n";
}

void test_cnet_http2() {
    using namespace utils::cnet;

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
    cout << start.delta() << "\n";
    auto client = http2::create_client();
    std::int32_t first_id = 0, second_id = 0;
    bool done_first = false;
    bool done_second = false;
    size_t expected_sum = 0;
    http2::set_frame_write_callback(client, [](const h2::Frame* fr) {
        print_frame("send", fr);
    });
    http2::set_frame_read_callback(client, [&](http2::Frames* fr) {
        for (auto& frame : fr) {
            print_frame("recv", frame.get());
            http2::default_proc(fr, frame, http2::DefaultProc::all);
            if (http2::is_settings_ack(frame)) {
                std::map<std::string, std::string> h;
                http2::set_request(h, host, path);
                http2::write_header(fr, h, first_id);
                h.clear();
                http2::set_request(h, host, "/teapot");
                http2::write_header(fr, h, second_id);
            }
            if (first_id != 0 && second_id != 0) {
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
                        cout << field.first << ": " << field.second << "\n";
                    }
                    if (no_length) {
                        http2::write_window_update(fr, 0xffffff, 0);
                        http2::write_window_update(fr, 0xffffff, frame->id);
                    }
                }
                std::string data;
                if (frame->type == h2::FrameType::data) {
                    http2::read_data(fr, data, frame->id);
                }
                if (http2::is_close_stream_signal(frame)) {
                    done_first = done_first || frame->id == first_id;
                    done_second = done_second || frame->id == second_id;
                }
            }
        }
        if (done_first && done_second) {
            http2::write_goaway(fr, 0);
            http2::flush_write_buffer(fr);
            tcp::shutdown(tcp, tcp::SEND);
            return false;
        }
        return true;
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
