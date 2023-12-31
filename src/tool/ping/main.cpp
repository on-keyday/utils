/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <fnet/connect.h>
#include <fnet/ip/icmp.h>
#include <wrap/admin.h>
#include <coro/coro.h>
#include <chrono>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <format>
#define HAS_FORMAT
#endif
using namespace utils::fnet;
struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
};
auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();

auto now() {
    return std::chrono::system_clock::now();
}

using point = std::chrono::system_clock::time_point;

struct TimePoint {
    point begin;
    point end;
    bool failure = false;
    utils::byte ttl = 0;
};

struct Data {
    std::uint64_t i = 0;
    std::string target;
    std::uint64_t count = 0;
    Socket sock;
    std::vector<TimePoint> period;
};

struct Workers {
    std::uint64_t count = 0;
    std::vector<Data> targets;
    size_t limit = 4;
};

void do_ping(utils::coro::C* ctx, Data* p) {
    auto wk = static_cast<Workers*>(ctx->get_thread_context());
    icmp::ICMPEcho hdr;
    hdr.type = icmp::Type::echo;
    hdr.identifier = p->i;
    hdr.seq_number = 1;
    utils::byte buffer[60];
    utils::binary::writer w{buffer};
    hdr.data = "buffer yes or no yahoo object !!";
    hdr.render_with_checksum(w).value();
    auto rem = p->sock.write(w.written());
    if (!rem) {
        cerr << rem.error().error<std::string>() << "\n";
        wk->count--;
        return;
    }
    p->period.push_back({now()});
    size_t current_count = p->count;
    BufferManager<std::string> buf_mgr;
    buf_mgr.buffer.resize(1200);
    auto res = p->sock.readfrom_async(
        utils::fnet::async_addr_then(buf_mgr, [=](Socket&& s, BufferManager<std::string> buf_mgr, NetAddrPort&& remote, NotifyResult&& r) {
            auto d = utils::helper::defer([&] {
                p->period.back().end = now();
                if (p->count + 1 < wk->limit) {
                    if (!ctx->add_coroutine(p, do_ping)) {
                        ctx->suspend();  // wait for other
                    }
                }
                else {
                    wk->count--;
                }
                p->count++;
            });
            auto val = r.readfrom_unwrap(buf_mgr.buffer, remote, [&](auto buf) { return s.readfrom(buf); });
            if (!val) {
                cerr << val.error().error<std::string>() << "\n";
                p->period.back().failure = true;
                return;
            }
            utils::binary::reader rd{val->first};
            ip::IPv4Header hdr;
            icmp::ICMPEcho reply;
            auto res = hdr.parse_with_checksum(rd)
                           .and_then([&] {
                               return reply.parse_with_checksum(rd);
                           });

            if (!res) {
                cerr << res.error().error<std::string>() << "\n";
                p->period.back().failure = true;
                return;
            }
            p->period.back().ttl = hdr.ttl;
            auto text = ipv4(hdr.src_addr.addr, 0).addr.to_string<std::string>();
            cout << utils::wrap::packln("ping to ", p->target, " (", text, ")", " succeeded");
        }));
    if (!res) {
        cerr << res.error().error<std::string>() << "\n";
        return;
    }
    if (res->state == NotifyState::done) {
        return;
    }
    auto timeout_period = now();
    while (now() - timeout_period < std::chrono::seconds(4)) {
        ctx->suspend();
        if (p->count != current_count) {
            return;
        }
    }
    res->cancel.cancel();
    while (p->count == current_count) {
        ctx->suspend();
    }
}

void ping(utils::coro::C* ctx, Data* p) {
    auto wk = static_cast<Workers*>(ctx->get_thread_context());
    auto attr = sockattr_raw(ip::Version::ipv4, ip::Protocol::icmp);
    auto c = connect_with(p->target, {}, attr, false, [&](WithState, auto&& wait) {
        for (;;) {
            auto res = wait(1);
            if (res || res.error() != error::block) {
                return res;
            }
            ctx->suspend();
        }
    });
    if (!c) {
        wk->count--;
        cerr << c.error().error<std::string>() << "\n";
        return;
    }
    std::tie(p->sock, std::ignore) = std::move(*c);
    do_ping(ctx, p);
}

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    if (flags.args.size() == 0) {
        cerr << "need host name";
        return -1;
    }
    auto result = utils::wrap::run_this_as_admin();
    if (result != utils::wrap::RunResult::already_admin) {
        if (result != utils::wrap::RunResult::started) {
            cerr << "need admin";
        }
        return 0;
    }
    auto co = utils::coro::make_coro(100, 100);
    if (!co) {
        cerr << "cannot make coroutine\n";
        return -1;
    }
    Workers wk;
    for (auto& arg : flags.args) {
        wk.count++;
        wk.targets.push_back({wk.count, std::move(arg)});
    }
    for (auto& target : wk.targets) {
        co.add_coroutine(
            &target, ping);
    }
    co.add_coroutine(nullptr, [](utils::coro::C* c, void*) {
        auto w = static_cast<Workers*>(c->get_thread_context());
        while (w->count) {
            wait_io_event(1);
            c->suspend();
        }
    });
    co.set_thread_context(&wk);
    while (co.run()) {
    }
    for (auto& target : wk.targets) {
        cout << "statics of " << target.target;
        if (target.sock) {
            cout << "(" << target.sock.get_remote_addr().value().addr.to_string<std::string>()
                 << ")";
        }
        else {
            cout << "(not resolved)";
        }
        cout << "\n";
        for (auto p : target.period) {
            if (!p.failure) {
#ifdef HAS_FORMAT
                cout << std::format("rtt: {} ttl: {}\n", std::chrono::duration_cast<std::chrono::milliseconds>(p.end - p.begin), p.ttl);
#else
                cout << "rtt: " << std::chrono::duration_cast<std::chrono::milliseconds>(p.end - p.begin).count() << "ms ttl: " << p.ttl;
#endif
            }
            else {
                cout << "ping failure\n";
            }
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
