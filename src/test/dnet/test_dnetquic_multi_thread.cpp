/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/quic/context/context.h>
#include <mutex>
#include <thread>
#include <random>
#include <dnet/socket.h>
#include <dnet/addrinfo.h>
#include <dnet/plthead.h>
#include <dnet/debug.h>
#include <dnet/quic/log/frame_log.h>
#include <dnet/quic/log/crypto_log.h>
#include <testutil/alloc_hook.h>
#include <wrap/cout.h>
#include <dnet/quic/path/dplpmtud.h>
using Context = utils::dnet::quic::context::Context<std::mutex>;
using QCTX = std::shared_ptr<Context>;
using namespace utils::dnet;

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
    constexpr auto k = utils::dnet::quic::crypto::make_key_from_bintext("040000b800093a805e1901820000a33d58b31fca95ec541ca0962e576116497101db810facc68c15d127241584483cb0d695a1ac53a41386346531571f4875747b15801be665888564ab81d5c5c1f66ba5d0f24022f8609fe4df43017dd843098a209a2d03bdc9d42c58eb3fbb2375a448c8267a0887bf6832754b470c470d26d0604b9a6ad864ea3096168a0bfed687b2efc4035c4b7bd75688ac18e4c815dbff703b46841e9a82447414378c8a5475c8a10008002a0004ffffffff");
}

void log_packet(std::shared_ptr<void>&, utils::dnet::quic::packet::PacketSummary su, utils::view::rvec payload) {
    std::string res = to_string(su.type);
    res += " src:";
    for (auto s : su.srcID) {
        utils::number::to_string(res, s, 16);
    }
    res += " dst:";
    for (auto d : su.dstID) {
        utils::number::to_string(res, d, 16);
    }
    utils::wrap::cout_wrap() << res << "\n";
    utils::io::reader r{payload};
    utils::dnet::quic::frame::parse_frames<slib::vector>(r, [](const auto& frame, bool) {
        std::string data;
        utils::dnet::quic::log::format_frame<slib::vector>(data, frame, false, false);
        data += "\n";
        if constexpr (std::is_same_v<const utils::dnet::quic::frame::CryptoFrame&, decltype(frame)>) {
            utils::dnet::quic::log::format_crypto(data, frame.crypto_data, false, false);
        }
        utils::wrap::cout_wrap() << data;
    });
}

utils::dnet::quic::log::LogCallbacks cbs{
    .debug = [](std::shared_ptr<void>&, const char* msg) { utils::wrap::cout_wrap() << msg << "\n"; },
    .sending_packet = log_packet,
    .recv_packet = log_packet,
};

int main() {
    utils::dnet::set_normal_allocs(utils::dnet::debug::allocs());
    utils::dnet::set_objpool_allocs(utils::dnet::debug::allocs());
    utils::test::log_hooker = [](utils::test::HookInfo info) {
        if (info.size == 232) {
        }
    };
    utils::test::set_alloc_hook(true);
    utils::test::set_log_file("memuse.log");
    tls::set_libcrypto(dnet_lazy_dll_path("D:/quictls/boringssl/built/lib/crypto.dll"));
    tls::set_libssl(dnet_lazy_dll_path("D:/quictls/boringssl/built/lib/ssl.dll"));
    QCTX ctx = std::make_shared<Context>();
    auto config = tls::configure();
    config.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    ctx->init_tls(config);
    ctx->crypto.tls.set_alpn("\4test");
    ctx->connIDs.issuer.user_gen_random = [](std::shared_ptr<void>&, utils::view::wvec data) {
        std::random_device dev;
        std::uniform_int_distribution uni(0, 255);
        for (auto& d : data) {
            d = uni(dev);
        }
        return true;
    };
    ctx->conf.version = quic::version_1;
    ctx->ackh.clock = quic::time::Clock{
        nullptr,
        1,
        [](void*) {
            return quic::time::Time(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        },
    };
    ctx->logger.callbacks = &cbs;
    auto val = ctx->connect_start();
    assert(val);

    auto wait = resolve_address("localhost", "8090", {.socket_type = SOCK_DGRAM});
    auto info = wait.wait();
    assert(!wait.failed());
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
    byte buf[3000];
    while (true) {
        std::tie(data, val) = ctx->create_udp_payload();
        if (!val && ctx->conn_err == utils::dnet::quic::ack::errIdleTimeout) {
            break;
        }
        assert(val);
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
