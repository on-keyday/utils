/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <cassert>
#include <string>
#include <fnet/util/http/http_headers.h>
#include <map>

using namespace utils;
struct SockHolder {
    fnet::Socket sock;
    // fnet::Socket::completion_recv_t comp;
    bool end;
    std::string str;
    utils::http::header::StatusCode code;
    std::string body;
    std::multimap<std::string, std::string> header;
};

int main() {
    fnet::SockAddr addr{};
    auto [resolve, err1] = fnet::resolve_address("www.google.com", "http", fnet::sockattr_tcp());
    assert(!err1);
    auto [list, err2] = resolve.wait();
    assert(!err2);
    fnet::Socket sock;
    while (list.next()) {
        auto addr = list.sockaddr();
        auto tmp = fnet::make_socket(addr.attr);
        if (!tmp) {
            continue;
        }

        auto err = tmp.connect(addr.addr);
        if (!err) {
            goto END;
        }
        if (fnet::isSysBlock(err)) {
            if (!tmp.wait_writable(10, 0).is_error()) {
                goto END;
            }
        }

        continue;
    END:
        sock = std::move(tmp);
        break;
    }
    assert(sock);
    constexpr auto data = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
    if (auto [_, err] = sock.write(data); err) {
        return -1;
    }
    SockHolder holder{std::move(sock)};
    auto completion = [](void* user, void* data, size_t len, size_t bufmax, int err) {
        auto h = static_cast<SockHolder*>(user);
        h->str.append((const char*)data, len);
        auto read = [&] {
            size_t size = len;
            char buf[2048]{};
            while (true) {
                auto [read, err] = h->sock.read(buf);
                if (err) {
                    break;
                }
                h->str.append(read.as_char(), read.size());
                len += read.size();
            }
        };
        if (len == bufmax) {
            read();
        }
        utils::http::header::read_response<std::string>(h->str, h->code, h->header, h->body, [&](auto& seq, size_t expect, bool end_call) {
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
