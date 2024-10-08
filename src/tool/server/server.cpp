/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <fnet/server/state.h>
#include <fnet/server/httpserv.h>
#include <wrap/cin.h>
#include <thread>
#include <json/to_string.h>
#include <json/json_export.h>
#include <fnet/server/format_state.h>
#include <mutex>
#include <helper/pushbacker.h>
#include <testutil/alloc_hook.h>
#include <fnet/debug.h>
#include <env/env_sys.h>
#include <chrono>
#include <format>
#include <timer/clock.h>
#include <timer/to_string.h>
#include "flags.h"

namespace serv = futils::fnet::server;
auto& cout = futils::wrap::cout_wrap();

void http_serve(void*, futils::fnet::server::Requester req, futils::fnet::server::StateContext s) {
    bool keep_alive;
    std::string method, path;
    req.http.peek_request_line(method, path);
    s.log(serv::log_level::info, req.client.addr, "request ", method, " ", path);
    serv::read_header_and_check_keep_alive<std::string>(
        req.http, futils::helper::nop, futils::helper::nop, [](auto&&...) {}, keep_alive);
    std::map<std::string, std::string> h;
    h["Content-Type"] = "text/html; charset=UTF-8";
    if (keep_alive) {
        h["Connection"] = "keep-alive";
    }
    else {
        h["Connection"] = "close";
    }
    req.respond_flush(s, serv::StatusCode::http_ok, h, "<h1>hello world</h1>\n");
    if (keep_alive) {
        serv::handle_keep_alive(std::move(req), std::move(s));
    }
}

std::mutex m;
std::deque<futils::wrap::path_string> ac;
std::mutex l;
std::deque<futils::wrap::path_string> lg;

void log_thread() {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (l.try_lock()) {
            if (!lg.size()) {
                l.unlock();
                continue;
            }
            auto vec = std::move(lg);
            l.unlock();
            for (auto& v : vec) {
                cout << v;
            }
        }
    }
}
constexpr auto timeFmt = "%Y-%M-%D %h:%m:%s ";
void log(auto&&... msg) {
    auto str = futils::timer::to_string<std::string>(timeFmt, futils::timer::utc_clock.now());
    auto r = futils::wrap::packln(str, msg...).raw();
    m.lock();
    ac.push_back(std::move(r));
    m.unlock();
}

void log_internal(auto&&... msg) {
    auto str = futils::timer::to_string<std::string>(timeFmt, futils::timer::utc_clock.now());
    auto r = futils::wrap::packln(str, msg...).raw();
    cout << r;
}

void server_entry(void* p, serv::Client&& cl, serv::StateContext ctx) {
    ctx.log(serv::log_level::info, "accept", cl.addr);
    serv::http_handler(p, std::move(cl), ctx);
}

int quic_server(Flags& flag);
bool verbose = false;

int server_main(Flags& flag, futils::cmdline::option::Context& ctx) {
    if (flag.memory_debug) {
        futils::fnet::debug::allocs();
        futils::test::set_alloc_hook(true);
    }
    if (flag.quic) {
        return quic_server(flag);
    }
    serv::HTTPServ serv;
    if (flag.ssl) {
        if (flag.libssl.size()) {
            futils::fnet::tls::set_libssl(flag.libssl.c_str());
        }
        if (flag.libcrypto.size()) {
            futils::fnet::tls::set_libcrypto(flag.libcrypto.c_str());
        }
        if (flag.public_key.size() && flag.private_key.size()) {
            auto conf = futils::fnet::tls::configure_with_error();
            if (!conf) {
                cout << "failed to configure tls " << conf.error().error<std::string>();
                return -1;
            }
            serv.tls_config = std::move(conf.value());
            auto r = serv.tls_config.set_cert_chain(flag.public_key.c_str(), flag.private_key.c_str());
            if (!r) {
                cout << "failed to load cert " << r.error().error<std::string>();
                return -1;
            }
        }
        else {
            cout << "no cert specified\n";
            return -1;
        }
    }
    serv.next = http_serve;
    verbose = flag.verbose;
    auto s = std::make_shared<serv::State>(&serv, server_entry);
    s->set_log([](serv::log_level level, futils::fnet::NetAddrPort* addr, futils::fnet::error::Error& err) {
        if (!verbose && level < serv::log_level::info) {
            return;
        }
        log(addr ? addr->to_string<std::string>(true, true) : "<no address>", " ",
            err.error());
    });
    s->set_max_and_active(std::thread::hardware_concurrency() - 1, 5);
    s->set_reduce_skip(10);
    auto server = serv::prepare_listener(flag.port, 10000, true, false, flag.bind_public ? futils::view::rvec{} : "localhost");
    if (!server) {
        futils::wrap::cout_wrap() << "failed to create server " << server.error().error<std::string>();
        return -1;
    }
    auto& servstate = s->state();
    futils::wrap::path_string str;
    auto _ = futils::file::File::stdin_file().interactive_console_read();
    auto input_callback = [&] {
        if (!servstate.busy() && m.try_lock()) {
            if (ac.size()) {
                std::thread([ac = std::move(ac)] {
                    futils::wrap::path_string p;
                    for (auto it = ac.begin(); it != ac.end(); it++) {
                        p += *it;
                    }
                    l.lock();
                    lg.push_back(std::move(p));
                    l.unlock();
                }).detach();
            }
            m.unlock();
        }
        futils::wrap::input(str, true);
        if (!str.size()) {
            return true;
        }
        if (str.back() == '\n') {
            str.pop_back();
            if (str == futils::utf::convert<futils::wrap::path_string>("status")) {
                cout << serv::format_state<std::string>(servstate);
            }
            str.clear();
        }
        else if (str.back() == 3) {
            log_internal("stopping server");
            s->notify();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return false;
        }
        return true;
    };
    log_internal("running server on port ", flag.port);
    std::thread(log_thread).detach();
    if (flag.single_thread) {
        s->serve(server->first, input_callback);
        return 0;
    }
    s->add_accept_thread(std::move(server->first));
    while (input_callback()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    log_internal("server stopped");
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    flags.load_env();
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return server_main(flags, ctx);
        });
}
