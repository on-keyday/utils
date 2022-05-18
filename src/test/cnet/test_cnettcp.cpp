/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/tcp.h>
#include <cnet/ssl.h>
#include <cassert>
#include <cstring>
#include <thread>
#include <wrap/light/string.h>
#include <wrap/cout.h>
#include <net/http/http_headers.h>
#include <wrap/light/hash_map.h>
#include <number/array.h>
#include <testutil/timer.h>
#include <async/light/context.h>
#include <async/light/pool.h>
using namespace utils;
auto& cout = wrap::cout_wrap();

void test_tcp_cnet(async::light::Context<void> as, const char* host, const char* path, int index) {
    test::Timer local_timer;
    auto conn = cnet::tcp::create_client();
    auto cb = [&](cnet::CNet* ctx) {
        if (!cnet::protocol_is(ctx, "tcp")) {
            return true;
        }
        if (cnet::tcp::is_waiting(ctx)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            as.suspend();
            return false;
        }
        auto p = cnet::tcp::get_current_state(ctx);
        if (p == cnet::tcp::TCPStatus::start_resolving_name) {
            cout << wrap::pack(index, ":", std::this_thread::get_id(), ":start timer\n");
            local_timer.reset();
        }
        else if (p == cnet::tcp::TCPStatus::resolve_name_done) {
            cout << wrap::pack(index, ":", std::this_thread::get_id(), ":dns resolving:", local_timer.delta(), "\n");
        }
        else if (p == cnet::tcp::TCPStatus::connected) {
            cout << wrap::pack(index, ":", std::this_thread::get_id(), ":tcp connecting:", local_timer.delta(), "\n");
        }
        as.suspend();
        return true;
    };
    cnet::set_lambda(conn, cb);
    cnet::tcp::set_hostport(conn, host, "https");
    auto sb = cnet::open(conn);
    auto ssl = cnet::ssl::create_client();
    auto cb2 = [&](cnet::CNet* ctx) {
        if (!cnet::protocol_is(ctx, "tls")) {
            return true;
        }
        auto cs = cnet::ssl::get_current_state(ctx);
        if (cs == cnet::ssl::TLSStatus::connected) {
            cout << wrap::pack(index, ":", std::this_thread::get_id(), ":ssl connecting:", local_timer.delta(), "\n");
        }
        as.suspend();
        return true;
    };
    cnet::set_lambda(ssl, cb2);
    auto suc = cnet::set_lowlevel_protocol(ssl, conn);
    assert(suc);
    cnet::ssl::set_certificate_file(ssl, "src/test/net/cacert.pem");
    cnet::ssl::set_host(ssl, host);
    auto err = cnet::open(ssl);
    if (!err) {
        cnet::ssl::error_callback(ssl, [](const char* msg, size_t len) {
            cout << helper::SizedView(msg, len);
        });
        assert(suc);
    }
    conn = ssl;
    auto data = "GET " + wrap::string(path) + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
    size_t w = 0;
    cnet::write(conn, data.c_str(), data.size(), &w);
    wrap::string buf, body;
    wrap::hash_multimap<wrap::string, wrap::string> h;
    suc = net::h1header::read_response<wrap::string>(buf, helper::nop, h, body, [&](auto& seq, size_t expect, bool finalcall) {
        number::Array<1024, char> tmpbuf{0};
        size_t r = 0;
        while (!r) {
            suc = cnet::read(conn, tmpbuf.buf, tmpbuf.capacity(), &r);
            if (!suc) {
                cout << "error:" << cnet::get_error(conn) << "\n";
                cnet::ssl::error_callback(conn, [](const char* msg, size_t len) {
                    cout << helper::SizedView(msg, len);
                });
                return false;
            }
        }
        as.suspend();
        seq.buf.buffer.append(tmpbuf.buf, r);
        return true;
    });
    if (!suc) {
        cout << "error:" << cnet::get_error(conn) << "\n";
        return;
    }
    auto d = local_timer.delta();
    // cout << body << "\n";
    cout << wrap::pack(index, ":", std::this_thread::get_id(), ":http responding:", d, "\n");
    cnet::delete_cnet(conn);
    cout << wrap::pack(index, ":", std::this_thread::get_id(), ":context deleted:", local_timer.delta(), "\n");
}

void task_run(wrap::shared_ptr<async::light::TaskPool> pool) {
    async::light::SearchContext<async::light::Task*> sctx{0};
    async::light::TimeContext tctx;
    tctx.limit = std::chrono::milliseconds(2000);
    tctx.update_delta = std::chrono::milliseconds(50);
    tctx.wait = std::chrono::milliseconds(10);
    while (true) {
        pool->run_task_with_wait(
            tctx, [](async::light::TimeContext<>& tctx) {
                auto hit = tctx.hit_delta();
                auto not_hit = tctx.not_hit_delta();
                if (not_hit > hit) {
                    tctx.wait *= 2;
                }
                if (tctx.wait > tctx.limit) {
                    tctx.wait = tctx.limit;
                }
            },
            &sctx);
    }
}

int main() {
    auto pool = wrap::make_shared<async::light::TaskPool>();
    int index = 0;
    auto spawn = [&](const char* host, const char* path) {
        pool->append(async::light::start<void>(true, test_tcp_cnet, host, path, std::move(index)));
        index++;
    };
    for (auto i = 0; i < std::thread::hardware_concurrency(); i++) {
        std::thread(task_run, pool).detach();
    }
    spawn("gmail.com", "/");
    spawn("www.google.com", "/");
    spawn("amazon.com", "/");
    spawn("stackoverflow.com", "/");
    spawn("go.dev", "/");
    spawn("httpbin.org", "/get");
    spawn("syosetu.com", "/");
    spawn("docs.microsoft.com", "/");
    spawn("youtube.com", "/");
    test::Timer timer;
    while (pool->size()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    cout << "total\n"
         << timer.delta() << "\n";
}
