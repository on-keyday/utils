/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <dnet/socket.h>
#include <dnet/addrinfo.h>
#include <dnet/dll/sockdll.h>
#include <cassert>
#include <string>
#include <net_util/http/http_headers.h>
#include <map>

using namespace utils;
struct SockHolder {
    dnet::Socket sock;
    // dnet::Socket::completion_recv_t comp;
    bool end;
    std::string str;
    utils::net::h1header::StatusCode code;
    std::string body;
    std::multimap<std::string, std::string> header;
};

int main() {
    dnet::SockAddr addr{};
    addr.hostname = "www.google.com";
    addr.namelen = strlen(addr.hostname);
    addr.type = SOCK_STREAM;
    auto resolve = dnet::resolve_address(addr, "http");
    auto list = resolve.wait();
    assert(!resolve.failed());
    dnet::Socket sock;
    while (list.next()) {
        list.sockaddr(addr);
        auto tmp = dnet::make_socket(addr.af, addr.type, addr.proto);
        if (!tmp) {
            continue;
        }

        auto err = tmp.connect(addr.addr, addr.addrlen);
        if (!err) {
            goto END;
        }
        if (dnet::isSysBlock(err)) {
            if (!tmp.wait_writable(10, 0).is_error()) {
                goto END;
            }
        }

    FAILED:
        continue;
    END:
        sock = std::move(tmp);
        break;
    }
    assert(sock);
    constexpr auto data = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
    if (auto [_, err] = sock.write(data, strlen(data)); err) {
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
                auto [readsize, err] = h->sock.read(buf, 2048);
                if (err) {
                    break;
                }
                h->str.append(buf, readsize);
                len += readsize;
            }
        };
        if (len == bufmax) {
            read();
        }
        utils::net::h1header::read_response<std::string>(h->str, h->code, h->header, h->body, [&](auto& seq, size_t expect, bool end_call) {
            read();
            return true;
        });
    };
    /*
    holder.comp = completion;
    auto res = holder.sock.read_async(completion, &holder);
    assert(res);
    while (!dnet::wait_event(~0)) {
    }
    return 0;
    */
    holder.sock.read_async([](auto&&...) {}, std::move(holder), [](auto& c) -> dnet::Socket& { return c.sock; }, [](auto&&...) {});
}
