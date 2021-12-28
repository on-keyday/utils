/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/tcp/tcp.h"
#include "../../include/net/ssl/ssl.h"
#include "../../include/wrap/lite/string.h"
#include "../../include/wrap/cout.h"
#include "../../include/net/http/http1.h"
#include "../../include/wrap/cout.h"

#include <windows.h>

utils::net::IOClose make_ioclose(const char* host, const char* port, bool secure) {
    auto query = utils::net::query_dns(host, port);
    auto addr = query.get_address();
    while (!addr) {
        if (query.failed()) {
            assert(false && "dns query failed");
        }
        Sleep(10);
        addr = query.get_address();
    }
    auto result = utils::net::open(std::move(addr));
    auto sock = result.connect();
    while (!sock) {
        if (result.failed()) {
            assert(false && "connect socket failed");
        }
        Sleep(10);
        sock = result.connect();
    }
    if (!secure) {
        return sock;
    }
    auto sslres = utils::net::open(std::move(sock), "./src/test/net/cacert.pem");
    auto ssl = sslres.connect();
    while (!ssl) {
        if (sslres.failed()) {
            assert(false && "connect ssl failed");
        }
        Sleep(10);
        ssl = sslres.connect();
    }
    return ssl;
}

void test_http1() {
    auto ioclose = make_ioclose("www.google.com", "https", true);
    assert(ioclose);
    auto req = utils::net::request(std::move(ioclose), "www.google.com", "GET", "/", {});
    auto resp = req.get_response();
    while (!resp) {
        if (req.failed()) {
            assert(false && "request http1 failed");
        }
        Sleep(10);
        resp = req.get_response();
    }
    utils::wrap::cout_wrap() << resp.body();
}

int main() {
    test_http1();
}