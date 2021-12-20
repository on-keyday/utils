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

#include <windows.h>

void test_netcore() {
    auto query = utils::net::query_dns("google.com", "https");
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
    auto sslres = utils::net::open(std::move(sock), "../src/test/net/cacert.pem");
    auto ssl = sslres.connect();
    while (!ssl) {
        if (sslres.failed()) {
            assert(false && "connect ssl failed");
        }
        Sleep(10);
        ssl = sslres.connect();
    }
    auto msg = "GET / HTTP/1.1\r\nHost: google.com\r\n";
    ssl->write(msg, strlen(msg));
    utils::wrap::string str;
    Sleep(10);
    utils::net::read(str, *ssl);
    auto& cout = utils::wrap::cout_wrap();
    cout << str;
}

int main() {
    test_netcore();
}