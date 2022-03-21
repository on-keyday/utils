/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/tcp/tcp.h"
#include "../../include/net/ssl/ssl.h"
#include "../../include/wrap/light/string.h"
#include "../../include/wrap/cout.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#define Sleep sleep
#endif

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
    auto sslres = utils::net::open(std::move(sock), "./src/test/net/cacert.pem");
    auto ssl = sslres.connect();
    while (!ssl) {
        if (sslres.failed()) {
            assert(false && "connect ssl failed");
        }
        Sleep(10);
        ssl = sslres.connect();
    }
    auto msg = "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n";
    while (ssl->write(msg, strlen(msg), nullptr) == utils::net::State::running) {
        Sleep(1);
    }
    utils::wrap::string str;
    while (utils::net::read(str, *ssl) == utils::net::State::running) {
        Sleep(1);
    }
    auto& cout = utils::wrap::cout_wrap();
    cout << str;
}

int main() {
    test_netcore();
}
