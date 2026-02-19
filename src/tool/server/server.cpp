/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
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
#include <timer/clock.h>
#include <timer/to_string.h>
#include "flags.h"
#include <file/file_stream.h>

namespace serv = futils::fnet::server;
auto& cout = futils::wrap::cout_wrap();

struct HttpRequest {
    std::string method;
    std::string path;
    std::string host;
    std::map<std::string, std::string> headers;
};

std::string alt_svc;

void http_serve(void*, std::shared_ptr<futils::fnet::server::Requester>&& req, futils::fnet::server::StateContext s) {
    auto user_data = req->get_or_set_user_data<HttpRequest>([](auto&& ptr, auto&& setter) {
        if (!ptr) {
            ptr = std::make_shared<HttpRequest>();
            setter(ptr);
        }
        return ptr;
    });
    std::map<std::string, std::string> h;
    switch (req->http.state()) {
        case futils::fnet::http1::HTTPState::init:
        case futils::fnet::http1::HTTPState::first_line:
        case futils::fnet::http1::HTTPState::header: {
            if (auto err = req->http.read_request(
                    user_data->method, user_data->path,
                    futils::fnet::http1::default_header_callback<std::string>([&](std::string&& key, std::string&& value) {
                        if (futils::strutil::equal(key, "Host", futils::strutil::ignore_case())) {
                            user_data->host = std::move(value);
                        }
                        else {
                            user_data->headers.emplace(std::move(key), std::move(value));
                        }
                    }))) {
                if (futils::fnet::http::is_resumable(err)) {
                    // futils::fnet::server::wait_for_data(std::move(req), s);
                    return;
                }
                s.log(serv::log_level::warn, req->addr, err);
                return;
            }
            if (user_data->method != "GET") {
                req->respond(serv::StatusCode::http_bad_request, h, "only GET is allowed");
                return;
            }
            if (user_data->host.empty()) {
                req->respond(serv::StatusCode::http_bad_request, h, "no host header");
                return;
            }
            /*
            if (!req->http.read_ctx.follows_no_body_semantics()) {
                req->respond(serv::StatusCode::http_bad_request, h, "no body allowed");
                return;
            }
            */
            // here, must be GET so no body
            break;
        }
        default: {
            req->respond(serv::StatusCode::http_bad_request, h, "bad request");
            return;
        }
    }
    h["Content-Type"] = "text/html; charset=UTF-8";
    if (alt_svc.size()) {
        h["Alt-Svc"] = alt_svc;
    }
    req->respond(serv::StatusCode::http_ok, h, "<h1>hello world</h1>\n");
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

void log(futils::wrap::internal::Pack p) {
    auto str = futils::timer::to_string<std::string>(timeFmt, futils::timer::utc_clock.now());
    auto r = futils::wrap::packln(str, p.raw()).raw();
    m.lock();
    ac.push_back(std::move(r));
    m.unlock();
}

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

bool setup_http3_server(Flags& flag, std::shared_ptr<futils::fnet::server::State>& state, std::string& out_address);
bool verbose = false;
futils::file::FileStream<std::string, futils::file::File, std::mutex> key_log_file;
void key_log(futils::view::rvec line, void*) {
    futils::binary::writer w{key_log_file.get_write_handler(), &key_log_file};
    w.write(line);
    w.write("\n");
}

int server_main(Flags& flag, futils::cmdline::option::Context& ctx) {
    if (flag.memory_debug) {
        futils::fnet::debug::allocs();
        futils::test::set_alloc_hook(true);
    }
    serv::HTTPServ serv;
    if (flag.libssl.size()) {
        futils::fnet::tls::set_libssl(flag.libssl.c_str());
    }
    if (flag.libcrypto.size()) {
        futils::fnet::tls::set_libcrypto(flag.libcrypto.c_str());
    }
    if (flag.key_log.size() &&
        (flag.ssl || flag.quic)) {
        auto res = futils::file::File::open(flag.key_log, futils::file::O_CREATE_DEFAULT, futils::file::rw_perm);
        if (!res) {
            cout << "failed to open key log file " << res.error().error<std::string>();
            return -1;
        }
        key_log_file.file = std::move(*res);
    }
    if (flag.ssl) {
        if (flag.public_key.size() && flag.private_key.size()) {
            auto conf = futils::fnet::tls::configure_with_error();
            if (!conf) {
                cout << "failed to configure tls " << conf.error().error<std::string>() << "\n";
                return -1;
            }
            serv.tls_config = std::move(conf.value());
            auto r = serv.tls_config.set_cert_chain(flag.public_key.c_str(), flag.private_key.c_str());
            if (!r) {
                cout << "failed to load cert " << r.error().error<std::string>() << "\n";
                return -1;
            }
            serv.tls_config.set_alpn_select_callback(
                [](futils::fnet::tls::ALPNSelector& selector, void*) {
                    futils::view::rvec name;
                    while (selector.next(name)) {
                        if (name == "h2" || name == "http/1.1") {
                            selector.select(name);
                            return true;
                        }
                    }
                    return false;
                },
                nullptr);
            if (flag.key_log.size()) {
                serv.tls_config.set_key_log_callback(key_log, nullptr);
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
    auto max_thread = std::thread::hardware_concurrency() - 1;
    if (max_thread < 5) {
        max_thread = 5;
    }
    s->set_max_and_active(max_thread, 5);
    s->set_reduce_skip(10);
    constexpr auto uninit = futils::fnet::error::Error("not initialized", futils::fnet::error::Category::app);
    futils::fnet::expected<std::pair<futils::fnet::Socket, futils::fnet::SockAddr>>
        server = futils::fnet::unexpect(uninit),
        secure_server = futils::fnet::unexpect(uninit);
    if (flag.only_secure && (!flag.ssl && !flag.quic)) {
        cout << "only secure server without --ssl or --http3 is not allowed\n";
        return -1;
    }
    if (!flag.only_secure) {
        server = serv::prepare_listener(flag.port, 10000, true, false, flag.bind_public ? futils::view::rvec{} : "localhost");
        if (!server) {
            futils::wrap::cout_wrap() << "failed to create server " << server.error().error<std::string>() << "\n";
            return -1;
        }
        auto addr = server->first.get_local_addr();
        if (!addr) {
            futils::wrap::cout_wrap() << "failed to get local address " << addr.error().error<std::string>() << "\n";
            return -1;
        }
        serv.normal_port = addr->port().u16();
    }
    std::string alt_svc_h2, alt_svc_h3;
    if (flag.ssl) {
        secure_server = serv::prepare_listener(flag.secure_port, 10000, true, true, flag.bind_public ? futils::view::rvec{} : "localhost");
        if (!secure_server) {
            futils::wrap::cout_wrap() << "failed to create secure server " << secure_server.error().error<std::string>() << "\n";
            return -1;
        }
        auto addr = secure_server->first.get_local_addr();
        if (!addr) {
            futils::wrap::cout_wrap() << "failed to get local address " << addr.error().error<std::string>() << "\n";
            return -1;
        }
        serv.secure_port = addr->port().u16();
        alt_svc_h2 = "h2=\":" + futils::number::to_string<std::string>(serv.secure_port) + "\"; ma=2592000";
    }
    std::string quic_address;
    if (flag.quic) {
        if (!setup_http3_server(flag, s, quic_address)) {
            return -1;
        }
        alt_svc_h3 += "h3=\":" + flag.secure_port + "\"; ma=2592000";
    }
    if (alt_svc_h2.size() && alt_svc_h3.size()) {
        if (flag.prior_http3_alt_svc) {
            alt_svc = alt_svc_h3 + ", " + alt_svc_h2;
        }
        else {
            alt_svc = alt_svc_h2 + ", " + alt_svc_h3;
        }
    }
    else if (alt_svc_h2.size()) {
        alt_svc = alt_svc_h2;
    }
    else if (alt_svc_h3.size()) {
        alt_svc = alt_svc_h3;
    }
    auto& servstate = s->state();
    futils::wrap::path_string str;
    // avoid ctrl-c
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
        if (flag.no_input) {
            return true;
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
    if (server) {
        auto addr = server->first.get_local_addr();
        log_internal("running server on ", addr->to_string<std::string>());
    }
    if (secure_server) {
        auto addr = secure_server->first.get_local_addr();
        log_internal("running secure server on ", addr->to_string<std::string>());
    }
    if (flag.quic) {
        log_internal("running quic server on ", quic_address);
    }
    std::thread(log_thread).detach();
    if (flag.single_thread) {
        s->serve([&]() -> futils::fnet::expected<std::pair<futils::fnet::Socket, futils::fnet::NetAddrPort>> {
            if (server) {
                auto x = server->first.accept();
                if (x) {
                    return x;
                }
            }
            if (secure_server) {
                return secure_server->first.accept();
            }
            return futils::fnet::unexpect(futils::fnet::error::block);
        },
                 input_callback);
        return 0;
    }
    if (server) {
        s->add_accept_thread(std::move(server->first));
    }
    if (secure_server) {
        s->add_accept_thread(std::move(secure_server->first));
    }
    while (input_callback()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    log_internal("server stopped");
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return server_main(flags, ctx);
        });
}
