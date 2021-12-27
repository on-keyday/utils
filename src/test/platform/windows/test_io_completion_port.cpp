/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/io_completetion_port.h"

#include "../../../include/net/tcp/tcp.h"

#include <windows.h>

auto get_tcp() {
    auto query = utils::net::query_dns("localhost:8080", "https");
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
    return sock;
}

void test_io_completion_port() {
    auto iocp = utils::platform::windows::start_iocp();
    auto tcp = get_tcp();
    iocp->register_handler((void*)tcp->get_raw(), [](size_t size) {
        Sleep(10);
        return (void)0;
    });
}

int main() {
    test_io_completion_port();
}