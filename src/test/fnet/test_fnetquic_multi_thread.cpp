/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/context/context.h>
#include <mutex>
#include <thread>
#include <random>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <fnet/debug.h>
#include <fnet/quic/log/frame_log.h>
#include <fnet/quic/log/crypto_log.h>
#include <testutil/alloc_hook.h>
#include <wrap/cout.h>
#include <fnet/quic/path/dplpmtud.h>
#include <format>
using Context = utils::fnet::quic::context::Context<std::mutex>;
using Config = utils::fnet::quic::context::Config;
using QCTX = std::shared_ptr<Context>;
using namespace utils::fnet;

void thread(QCTX ctx) {
    while (true) {
        auto uni = ctx->streams->open_uni();
        if (!uni) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        uni->add_data(utils::view::rvec("hello world"), true);
        break;
    }
    constexpr auto k = utils::fnet::quic::crypto::make_key_from_bintext("040000b800093a805e1901820000a33d58b31fca95ec541ca0962e576116497101db810facc68c15d127241584483cb0d695a1ac53a41386346531571f4875747b15801be665888564ab81d5c5c1f66ba5d0f24022f8609fe4df43017dd843098a209a2d03bdc9d42c58eb3fbb2375a448c8267a0887bf6832754b470c470d26d0604b9a6ad864ea3096168a0bfed687b2efc4035c4b7bd75688ac18e4c815dbff703b46841e9a82447414378c8a5475c8a10008002a0004ffffffff");
}

void log_packet(std::shared_ptr<void>&, utils::fnet::quic::packet::PacketSummary su, utils::view::rvec payload, bool is_send) {
    std::string res = is_send ? "send: " : "recv: ";
    res += to_string(su.type);
    res += " ";
    utils::number::to_string(res, su.packet_number.value);
    res += " src:";
    for (auto s : su.srcID) {
        if (s < 0x10) {
            res += '0';
        }
        utils::number::to_string(res, s, 16);
    }
    res += " dst:";
    for (auto d : su.dstID) {
        if (d < 0x10) {
            res += '0';
        }
        utils::number::to_string(res, d, 16);
    }
    utils::wrap::cout_wrap() << res << "\n";
    utils::io::reader r{payload};
    utils::fnet::quic::frame::parse_frames<slib::vector>(r, [](const auto& frame, bool) {
        std::string data;
        utils::fnet::quic::log::format_frame<slib::vector>(data, frame, false, false);
        data += "\n";
        if constexpr (std::is_same_v<const utils::fnet::quic::frame::CryptoFrame&, decltype(frame)>) {
            utils::fnet::quic::log::format_crypto(data, frame.crypto_data, false, false);
        }
        utils::wrap::cout_wrap() << data;
    });
}
utils::fnet::quic::status::LossTimerState prev_state = utils::fnet::quic::status::LossTimerState::no_timer;
utils::fnet::quic::time::Time prev_deadline = {};
void record_timer(std::shared_ptr<void>&, const utils::fnet::quic::status::LossTimer& timer, utils::fnet::quic::time::Time now) {
    auto print = [&] {
        utils::wrap::cout_wrap() << std::format("loss timer: {} {} until deadline {}ms\n", to_string(timer.current_space()), to_string(prev_state), prev_deadline.value - now.value);
    };
    if (timer.current_state() != prev_state ||
        timer.deadline() != prev_deadline) {
        prev_state = timer.current_state();
        prev_deadline = timer.deadline();
        if (prev_state != utils::fnet::quic::status::LossTimerState::no_timer) {
            print();
        }
        else {
            utils::wrap::cout_wrap() << "loss timer: canceled\n";
        }
    }
}

utils::fnet::quic::log::LogCallbacks cbs{
    .debug = [](std::shared_ptr<void>&, const char* msg) { utils::wrap::cout_wrap() << msg << "\n"; },
    .sending_packet = log_packet,
    .recv_packet = log_packet,
    .loss_timer_state = record_timer,
};

int main() {
    utils::fnet::set_normal_allocs(utils::fnet::debug::allocs());
    utils::fnet::set_objpool_allocs(utils::fnet::debug::allocs());
    utils::test::log_hooker = [](utils::test::HookInfo info) {
        if (info.size == 232) {
        }
    };
    utils::test::set_alloc_hook(true);
    utils::test::set_log_file("memuse.log");
#ifdef _WIN32
    tls::set_libcrypto(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/crypto.dll"));
    tls::set_libssl(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/ssl.dll"));
#endif
    QCTX ctx = std::make_shared<Context>();
    Config config;
    config.tls_config = tls::configure();
    config.tls_config.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    config.tls_config.set_alpn("\4test");
    config.random = utils::fnet::quic::connid::Random{
        nullptr, [](std::shared_ptr<void>&, utils::view::wvec data) {
            std::random_device dev;
            std::uniform_int_distribution uni(0, 255);
            for (auto& d : data) {
                d = uni(dev);
            }
            return true;
        }};
    config.internal_parameters.clock = quic::time::Clock{
        nullptr,
        1,
        [](void*) {
            return quic::time::Time(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        },
    };
    config.internal_parameters.handshake_idle_timeout = 10000;
    config.logger.callbacks = &cbs;
    auto val = ctx->init(std::move(config)) &&
               ctx->connect_start();
    assert(val);

    auto wait = resolve_address("localhost", "8090", sockattr_udp());
    assert(!wait.second);
    auto [info, err] = wait.first.wait();
    assert(!err);
    Socket sock;
    NetAddrPort addr;
    while (info.next()) {
        auto saddr = info.sockaddr();
        sock = make_socket(saddr.attr);
        if (!sock) {
            continue;
        }
        addr = saddr.addr;
        break;
    }
    assert(sock);
    std::thread(thread, std::as_const(ctx)).detach();
    utils::view::rvec data;
    utils::byte buf[3000];
    while (true) {
        std::tie(data, val) = ctx->create_udp_payload();
        if (!val && utils::fnet::quic::context::is_timeout(ctx->conn_err)) {
            break;
        }
        if (!val) {
            utils::wrap::cout_wrap() << ctx->conn_err.error<std::string>() << "\n";
            assert(val);
        }
        if (data.size()) {
            sock.writeto(addr, data);
        }
        while (true) {
            auto [d, peer, err] = sock.readfrom(buf);
            if (isSysBlock(err)) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                break;
            }
            assert(!err);
            ctx->parse_udp_payload(d);
        }
    }
}
