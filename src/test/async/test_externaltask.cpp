/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/worker.h>
#include <wrap/cout.h>
#include <deprecated/net/tcp/tcp.h>
#include <deprecated/net/http/http1.h>
#include <deprecated/net/core/platform.h>
#include <platform/windows/io_completetion_port.h>
#include <chrono>
#include <thread>

void test_externaltask() {
    using namespace utils;
    using namespace std::chrono;
    async::TaskPool pool;
    //  pool.set_yield(false);
    struct OutParam {
        ::OVERLAPPED ol;
        const char* host_;
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
    std::atomic_size_t seqtime = 0;
    auto spawn = [&](const char* host, const char* path = "/") {
        return pool.start([=, &seqtime](async::Context& ctx) {
            auto out = utils::wrap::pack();
            auto begin = system_clock::now();
            auto q = net::query_dns(host, "http");
            auto r = q.get_address();
            auto prev = begin;
            while (!r) {
                if (q.failed()) {
                    assert(false && "failed");
                }
                ctx.suspend();
                out << host << ":wait dns...\n";
                auto n = system_clock::now();
                out << "delta:" << duration_cast<milliseconds>(n - prev) << "\n";
                prev = n;
                r = q.get_address();
            }
            auto op = net::open(std::move(r));
            auto c = op.connect();
            prev = system_clock::now();
            while (!c) {
                if (op.failed()) {
                    assert(false && "failed");
                }
                ctx.suspend();
                out << host << ":wait connect...\n";
                auto n = system_clock::now();
                out << "delta:" << duration_cast<milliseconds>(n - prev) << "\n";
                prev = n;
                c = op.connect();
            }
            ::SOCKET sock = (::SOCKET)c->get_raw();
            wrap::string text = "GET ";
            text += path;
            text += " HTTP/1.1\r\nHost: ";
            text += host;
            text += "\r\n\r\n";

            auto res = c->write(text.c_str(), text.size(), nullptr);
            assert(res == net::State::complete);
            auto iocp = platform::windows::get_iocp();
            iocp->register_handle((void*)sock);
            auto will = ctx.clone();
            OutParam param;
            param.host_ = host;
            param.f = std::move(will);  // pass external task
            auto err = ::WSARecv(sock, param.set_buf(), 1, (LPDWORD)&param.recved, (LPDWORD)&param.flags, &param.ol, nullptr);

            if (err == SOCKET_ERROR) {
                err = ::GetLastError();
                if (err == WSA_IO_PENDING) {
                    ctx.externaltask_wait();
                    out << "external job finished\n";
                }
            }
            auto end = system_clock::now();
            auto time = duration_cast<milliseconds>(end - begin);
            out << "time:" << time << "\n";
            out << param.alloc.c_str();
            utils::wrap::cout_wrap() << out.pack();
            seqtime += time.count();
        });
    };
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
                    utils::wrap::cout_wrap() << utils::wrap::packln("handling ", param->host_, "...");
                    req->complete();
                },
                8, INFINITE);
        }
    }).detach();
    auto begin = system_clock::now();
    auto g = spawn("google.com");
    auto s = spawn("syosetu.com");
    auto a = spawn("amazon.com");
    auto b = spawn("httpbin.org", "/get");
    auto u = spawn("youtube.com");
    auto v = spawn("go.dev");
    auto e = spawn("stackoverflow.com");
    auto w = spawn("ja.wikipedia.org");
    auto q = spawn("qiita.com");
    auto d = spawn("docs.microsoft.com");
    auto h = spawn("github.com");
    auto x = spawn("example.com");
    g.wait();
    s.wait();
    a.wait();
    b.wait();
    u.wait();
    v.wait();
    e.wait();
    w.wait();
    q.wait();
    d.wait();
    h.wait();
    x.wait();
    auto end = system_clock::now();
    auto time = duration_cast<milliseconds>(end - begin);
    utils::wrap::cout_wrap() << "seqtime: " << seqtime << "ms\n";
    utils::wrap::cout_wrap() << "totaltime: " << time << "\n";
}

int main() {
    test_externaltask();
}
