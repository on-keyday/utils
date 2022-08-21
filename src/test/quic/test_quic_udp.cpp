/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <quic/io/udp.h>
#include <cstdlib>
#include <cmdline/option/optcontext.h>
#include <wrap/cout.h>
#include <wrap/argv.h>
#include <thread>
#include <chrono>

auto& cout = utils::wrap::cout_wrap();

void test_quic_udp_client(const char* data, size_t len) {
    using namespace utils::quic;
    auto alloc = allocate::stdalloc();
    auto proto = io::udp::Protocol(&alloc);
    io::Target t = io::IPv4(127, 0, 0, 1, 8080);
    t.key = io::udp::UDP_IPv4;
    auto res = proto.new_target(&t, client);
    assert(ok(res));
    t.target = target_id(res);
    io::IPv6({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 8080);
    io::Option opt{};
    opt.key = io::udp::Connect;
    opt.target = t.target;
    opt.ptr = static_cast<void*>(&t);
    res = proto.option(&opt);
    assert(ok(res));
    opt.key = io::udp::MTU;
    res = proto.option(&opt);
    assert(ok(res));
    auto mtu = opt.rn;
    cout << "MTU:" << mtu << "\n";
    len = 1024 < len ? 1024 : len;
    auto begin = std::chrono::system_clock::now();
    proto.write_to(&t, reinterpret_cast<const byte*>(data), len);
    tsize wait = 0;

    while (true) {
        byte s[1025]{0};
        res = proto.read_from(&t, s, sizeof(s));
        if (incomplete(res)) {
            wait++;
            continue;
        }
        assert(ok(res));
        cout << s << "\n";
        cout << "wait: " << wait << "\n";
        cout << std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - begin)
                    .count();
        break;
    }
    proto.discard_target(t.target);
}

void test_quic_udp_server(const char* data, size_t len) {
    using namespace utils::quic;
    auto alloc = allocate::stdalloc();
    auto proto = io::udp::Protocol(&alloc);
    io::Target t = io::IPv4(127, 0, 0, 1, 8080);
    t.key = io::udp::UDP_IPv4;
    auto res = proto.new_target(&t, server);
    assert(ok(res));
    t.target = target_id(res);
    len = 1024 < len ? 1024 : len;
    while (true) {
        byte s[1025]{0};
        auto r = proto.read_from(&t, s, sizeof(s) - 1);
        if (incomplete(r)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        cout << s << "\n";
        proto.write_to(&t, reinterpret_cast<const byte*>(data), len);
    }
}

int main(int argc, char** argv) {
    using namespace utils::cmdline;
    utils::wrap::U8Arg _(argc, argv);
    option::Context c;
    bool* lauch_server = c.Bool("s,server", false, "launch as server");
    auto data = c.String<utils::wrap::string>("d,data", "hello", "sending data", "[data]");
    auto p = option::parse(argc, argv, c, utils::helper::nop, option::ParseFlag::assignable_mode & ~option::ParseFlag::parse_all);
    if (option::error_msg(p)) {
        return -1;
    }

    if (*lauch_server) {
        test_quic_udp_server(data->c_str(), data->size());
    }
    else {
        test_quic_udp_client(data->c_str(), data->size());
    }
}
