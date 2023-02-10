/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <dnet/server/state.h>
#include <dnet/server/httpserv.h>
#include <wrap/cin.h>
#include <thread>
#include <json/to_string.h>
#include <json/json_export.h>
#include <dnet/server/format_state.h>
#include <mutex>
#include <helper/pushbacker.h>

struct Flags : utils::cmdline::templ::HelpOption {};
namespace serv = utils::dnet::server;
auto& cout = utils::wrap::cout_wrap();

void http_serve(void*, utils::dnet::server::Requester req, utils::dnet::server::StateContext s) {
    std::map<std::string, std::string> h;
    const auto keep_alive = serv::has_keep_alive<std::string>(req.http, h);
    h.clear();
    h["Connection"] = "keep-alive";
    req.respond_flush(serv::http_ok, h, "hello world", 11);
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
            auto v = std::move(lg.front());
            lg.pop_front();
            l.unlock();
            cout << v;
        }
    }
}

void server_entry(void* p, serv::Client&& cl, serv::StateContext ctx) {
    auto r = utils::wrap::packln("accept ", cl.addr.to_string<std::string>(true, true)).raw();
    m.lock();
    ac.push_back(std::move(r));
    m.unlock();
    serv::http_handler(p, std::move(cl), ctx);
}

int server_main(Flags& flag, utils::cmdline::option::Context& ctx) {
    serv::HTTPServ serv;
    serv.next = http_serve;
    auto s = std::make_shared<serv::State>(&serv, server_entry);
    s->set_max_and_active(std::thread::hardware_concurrency() - 1, 5);
    s->set_reduce_skip(10);
    if (!serv::prepare_listeners(
            "8091", [&](auto&&, utils::dnet::Socket& prep) {
                s->add_accept_thread(std::move(prep));
            },
            2, 10000)) {
        utils::wrap::cout_wrap() << "failed to create server";
        return -1;
    }
    utils::wrap::cout_wrap() << "running server\n";
    utils::wrap::path_string str;
    std::thread(log_thread).detach();
    auto& servstate = s->state();
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
            continue;
        }
        if (str.back() == '\n') {
            str.pop_back();
            if (str == L"status") {
                utils::json::JSON js;
                cout << serv::format_state<std::string>(servstate, js);
            }
            str.clear();
        }
        else if (str.back() == 3) {
            cout << "stopping server\n";
            s->notify();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return server_main(flags, ctx);
        });
}
