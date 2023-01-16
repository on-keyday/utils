/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/http2/http2.h>
#include <map>
#include <string>

void test_dnet_http2() {
    utils::dnet::http2::HTTP2 client, server;
    client = utils::dnet::http2::create_http2(false, {.enable_push = false});
    server = utils::dnet::http2::create_http2(true, {.enable_push = false});
    namespace fr = utils::dnet::h2frame;
    auto s1 = client.create_stream();
    std::map<std::string, std::string> head;
    head[":method"] = "GET";
    head[":path"] = "/";
    head[":authority"] = "www.google.com";
    head[":scheme"] = "https";
    s1.write_header(head, true);
    char buf[1024]{};
    size_t red = 0;
    client.receive_http2_data(buf, sizeof(buf), &red);
    server.provide_http2_data(buf, red);
    auto scb = [](void* u, utils::dnet::http2::frame::Frame& f, utils::dnet::http2::stream::StreamNumState state) {
        auto serv = (decltype(server)*)u;
        auto s1 = serv->get_stream(f.id);
        bool res = false;
        if (state.com.state == utils::dnet::http2::stream::State::half_closed_remote) {
            std::map<std::string, std::string> head;
            head[":status"] = "200";
            head["server"] = "libdnet";
            res = s1.write_header(head, true);
        }
        assert(res);
    };
    server.set_stream_callback(scb, &server);
    auto res = server.progress();
    assert(res);
    server.receive_http2_data(buf, sizeof(buf), &red);
    client.provide_http2_data(buf, red);
    auto ccb = [](void* u, utils::dnet::http2::frame::Frame& f, utils::dnet::http2::stream::StreamNumState state) {
        auto clie = (decltype(client)*)u;
        auto s = state;
        if (s.com.state != utils::dnet::http2::stream::State::closed) {
            assert(false);
        }
    };
    client.set_stream_callback(ccb, &client);
    res = client.progress();
    assert(res);
}

int main() {
    test_dnet_http2();
}
