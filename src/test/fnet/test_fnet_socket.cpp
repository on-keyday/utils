/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <cassert>
#include <string>
#include <fnet/util/http/http_headers.h>
#include <fnet/connect.h>
#include <map>

using namespace futils;
struct SockHolder {
    fnet::Socket sock;
    // fnet::Socket::completion_recv_t comp;
    bool end;
    std::string str;
    futils::http::header::StatusCode code;
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
        futils::http::header::read_response<std::string>(h->str, h->code, h->header, h->body, [&](auto& seq, size_t expect, bool end_call) {
            read();
            return true;
        });
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
