/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/worker.h>
#include <wrap/cout.h>
#include <net/tcp/tcp.h>
#include <net/core/platform.h>
#include <platform/windows/io_completetion_port.h>

void test_externaltask() {
    using namespace utils;
    async::TaskPool pool;
    struct OutParam {
        ::OVERLAPPED ol;
        async::AnyFuture f;
        std::uint32_t recved;
        std::uint32_t flags;
        wrap::string alloc;
        WSABUF buf;
        WSABUF* set_buf() {
            alloc.resize(1024);
            buf.buf = alloc.data();
            buf.len = alloc.size();
            return &buf;
        }
    };
    auto v = pool.start([](async::Context& ctx) {
        auto q = net::query_dns("google.com", "http");
        auto r = q.get_address();
        while (!r) {
            if (q.failed()) {
                assert(false && "failed");
            }
            r = q.get_address();
        }
        auto op = net::open(std::move(r));
        auto c = op.connect();
        while (!c) {
            if (op.failed()) {
                assert(false && "failed");
            }
            c = op.connect();
        }
        ::SOCKET sock = (::SOCKET)c->get_raw();
        constexpr auto text = "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n";
        auto res = c->write(text, ::strlen(text));
        assert(res == net::State::complete);
        auto iocp = platform::windows::get_iocp();
        iocp->register_handle((void*)sock);
        auto will = ctx.clone();
        OutParam param;
        param.f = std::move(will);
        auto err = ::WSARecv(sock, param.set_buf(), 1, (LPDWORD)&param.recved, (LPDWORD)&param.flags, &param.ol, nullptr);
        if (err == SOCKET_ERROR) {
            err = ::GetLastError();
            if (err == WSA_IO_PENDING) {
                ctx.wait_externaltask();
                utils::wrap::cout_wrap() << "external job finished\n";
            }
        }
        utils::wrap::cout_wrap() << param.alloc;
    });
    std::thread([&]() {
        auto iocp = platform::windows::get_iocp();
        while (true) {
            iocp->wait_completion(
                [](void* ol, size_t count) {
                    if (!ol) {
                        return;
                    }
                    auto param = (OutParam*)ol;
                    auto req = param->f.get_taskrequest();
                    req->complete();
                },
                8, INFINITE);
        }
    }).detach();
    v.wait();
}

int main() {
    test_externaltask();
}
