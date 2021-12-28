/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/io_completetion_port.h"

#include "../../../include/net/tcp/tcp.h"

#include "../../../include/helper/pushbacker.h"

#include <WinSock2.h>

auto get_tcp() {
    auto query = utils::net::query_dns("localhost", "8080");
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
    ::SOCKET sock = tcp->get_raw();
    bool sent = false;
    iocp->register_handler(reinterpret_cast<void*>(sock), [&](size_t size) {
        sent = true;
    });
    utils::helper::FixedPushBacker<char[50], 50> buf;
    utils::helper::append(buf, "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n");
    ::WSABUF wsbuf;
    wsbuf.buf = buf.buf;
    wsbuf.len = buf.size();
    ::OVERLAPPED ol{0};
    ol.hEvent = ::CreateEventW(nullptr, true, false, nullptr);
    auto res = ::WSASend(sock, &wsbuf, 1, nullptr, 0, &ol, nullptr);
    auto err = ::WSAGetLastError();
    assert(err == 0);
    while (!sent) {
        Sleep(100);
    }
    sent = false;
    buf = {};
    ::CloseHandle(ol.hEvent);
    utils::wrap::string str;
    str.resize(1024);
    auto wsbuf2 = utils::wrap::make_shared<WSABUF>();
    auto ol2 = utils::wrap::make_shared<OVERLAPPED>();
    wsbuf2->buf = str.data();
    wsbuf2->len = 1024;
    ol2->hEvent = ::CreateEventW(nullptr, true, false, nullptr);
    res = ::WSARecv(sock, utils::helper::deref(wsbuf2), 1, nullptr, 0,
                    utils::helper::deref(ol2), nullptr);
    err = ::WSAGetLastError();
    while (!sent) {
        Sleep(100);
    }
}

int main() {
    test_io_completion_port();
}
