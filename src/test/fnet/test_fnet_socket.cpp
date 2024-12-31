/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <cassert>
#include <string>
#include <fnet/http1/header.h>
#include <fnet/http1/body.h>
#include <fnet/connect.h>
#include <map>

using namespace futils;
struct SockHolder {
    fnet::Socket sock;
    // fnet::Socket::completion_recv_t comp;
    bool end;
    std::string str;
    futils::fnet::http1::header::StatusCode code;
    std::string body;
    std::multimap<std::string, std::string> header;
};

int main() {
    fnet::SockAddr addr{};
    auto sock = fnet::connect("www.google.com", "http", fnet::sockattr_tcp()).value().first;
    constexpr auto data = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
    sock.write(data).value();
    SockHolder holder{std::move(sock)};
    auto completion = [](void* user, void* data, size_t len, size_t bufmax, int err) {
        auto h = static_cast<SockHolder*>(user);
        h->str.append((const char*)data, len);
        auto read = [&] {
            size_t size = len;
            char buf[2048]{};
            while (true) {
                auto data = h->sock.read(buf);
                if (!data) {
                    break;
                }
                h->str.append(data->as_char(), data->size());
                len += data->size();
            }
        };
        if (len == bufmax) {
            read();
        }
        futils::fnet::http1::ReadContext ctx;
        auto seq = futils::make_ref_seq(h->str);
        while (true) {
            auto err = futils::fnet::http1::header::parse_response(ctx, seq, futils::helper::nop, h->code, helper::nop, futils::fnet::http1::default_header_callback<std::string>(h->header));
            if (!err) {
                if (ctx.is_resumable()) {
                    read();
                    continue;
                }
            }
            break;
        }
    };
    /*
    holder.comp = completion;
    auto res = holder.sock.read_async(completion, &holder);
    assert(res);
    while (!fnet::wait_event(~0)) {
    }
    return 0;
    */
    // holder.sock.read_async([](auto&&...) {}, std::move(holder), [](auto& c) -> fnet::Socket& { return c.sock; }, [](auto&&...) {});
}
