/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

struct Flags : utils::cmdline::templ::HelpOption {
    std::string port = "8091";
    bool quic = false;
    bool single_thread = false;
    bool memory_debug = false;
    bool verbose = false;
    std::string public_key;
    std::string private_key;
    utils::wrap::path_string libssl;
    utils::wrap::path_string libcrypto;
    bool ssl = false;
    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        load_env();
        ctx.VarString(&port, "port", "port number (default:8091)", "PORT");
        ctx.VarBool(&quic, "quic", "enable quic server");
        ctx.VarBool(&single_thread, "single", "single thread mode");
        ctx.VarBool(&memory_debug, "memory-debug", "add memory debug hook");
        ctx.VarBool(&verbose, "verbose", "verbose log");
        ctx.VarString(&public_key, "public-key", "public key file", "FILE");
        ctx.VarString(&private_key, "private-key", "private key file", "FILE");
        ctx.VarString(&libssl, "libssl", "libssl path", "FILE");
        ctx.VarString(&libcrypto, "libcrypto", "libcrypto path", "FILE");
        ctx.VarBool(&ssl, "ssl", "enable ssl");
    }

    void load_env() {
        auto env = utils::env::sys::env_getter();
        libssl = env.get_or<utils::wrap::path_string>("FNET_LIBSSL", "");
        libcrypto = env.get_or<utils::wrap::path_string>("FNET_LIBCRYPTO", "");
        public_key = env.get_or<std::string>("FNET_PUBLIC_KEY", "");
        private_key = env.get_or<std::string>("FNET_PRIVATE_KEY", "");
    }
};
namespace serv = utils::fnet::server;
auto& cout = utils::wrap::cout_wrap();

void http_serve(void*, utils::fnet::server::Requester req, utils::fnet::server::StateContext s) {
    bool keep_alive;
    serv::read_header_and_check_keep_alive<std::string>(
        req.http, [](auto&&...) {}, keep_alive);
    std::map<std::string, std::string> h;
    if (keep_alive) {
        h["Connection"] = "keep-alive";
    }
    else {
        h["Connection"] = "close";
    }
    req.respond_flush(s, serv::StatusCode::http_ok, h, "hello world\n");
    if (keep_alive) {
        serv::handle_keep_alive(std::move(req), std::move(s));
    }
}

std::mutex m;
std::deque<utils::wrap::path_string> ac;
std::mutex l;
std::deque<utils::wrap::path_string> lg;

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

void log(auto&&... msg) {
    auto r = utils::wrap::packln(msg...).raw();
    m.lock();
    ac.push_back(std::move(r));
    m.unlock();
}

void server_entry(void* p, serv::Client&& cl, serv::StateContext ctx) {
    ctx.log(serv::log_level::info, "accept", cl.addr);
    serv::http_handler(p, std::move(cl), ctx);
}

int quic_server();
bool verbose = false;

int server_main(Flags& flag, utils::cmdline::option::Context& ctx) {
    if (flag.memory_debug) {
        utils::fnet::debug::allocs();
        utils::test::set_alloc_hook(true);
    }
    if (flag.quic) {
        return quic_server();
    }
    serv::HTTPServ serv;
    if (flag.ssl) {
        if (flag.libssl.size()) {
            utils::fnet::tls::set_libssl(flag.libssl.c_str());
        }
        if (flag.libcrypto.size()) {
            utils::fnet::tls::set_libcrypto(flag.libcrypto.c_str());
        }
        if (flag.public_key.size() && flag.private_key.size()) {
            auto conf = utils::fnet::tls::configure_with_error();
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
    s->set_log([](serv::log_level level, utils::fnet::NetAddrPort* addr, utils::fnet::error::Error& err) {
        if (!verbose && level < serv::log_level::info) {
            return;
        }
        log(addr ? addr->to_string<std::string>(true, true) : "<no address>", " ",
            err.error());
    });
    s->set_max_and_active(std::thread::hardware_concurrency() - 1, 5);
    s->set_reduce_skip(10);
    auto server = serv::prepare_listener(flag.port, 10000);
    if (!server) {
        utils::wrap::cout_wrap() << "failed to create server " << server.error().error<std::string>();
        return -1;
    }
    auto& servstate = s->state();
    utils::wrap::path_string str;
    auto _ = utils::file::File::stdin_file().interactive_console_read();
    auto input_callback = [&] {
        if (!servstate.busy() && m.try_lock()) {
            if (ac.size()) {
                std::thread([ac = std::move(ac)] {
                    utils::wrap::path_string p;
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
        utils::wrap::input(str, true);
        if (!str.size()) {
            return true;
        }
        if (str.back() == '\n') {
            str.pop_back();
            if (str == utils::utf::convert<utils::wrap::path_string>("status")) {
                utils::json::JSON js;
                cout << serv::format_state<std::string>(servstate, js);
            }
            str.clear();
        }
        else if (str.back() == 3) {
            cout << "stopping server\n";
            s->notify();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return false;
        }
        return true;
    };
    utils::wrap::cout_wrap() << "running server on port " << flag.port << " \n";
    std::thread(log_thread).detach();
    if (flag.single_thread) {
        s->serve(server->first, input_callback);
        return 0;
    }
    s->add_accept_thread(std::move(server->first));
    while (input_callback()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return server_main(flags, ctx);
        });
}
