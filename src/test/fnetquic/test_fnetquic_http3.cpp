/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/http3/unistream.h>
#include <fnet/http3/stream.h>
#include <fnet/http3/typeconfig.h>
#include <fnet/quic/quic.h>
#include <fnet/util/qpack/typeconfig.h>
#include <env/env_sys.h>
#include <fnet/addrinfo.h>
#include <fnet/socket.h>
#include <fnet/connect.h>
#include <condition_variable>
#include <thread>
#include <map>
#include <wrap/cout.h>
#include <fnet/quic/frame/parse.h>
#include <fnet/quic/log/frame_log.h>
#include <fnet/quic/log/crypto_log.h>
#include <vector>

using namespace futils::fnet::quic::use::smartptr;

using path = futils::wrap::path_string;
path libssl;
path libcrypto;
std::string cert;

void load_env() {
    auto env = futils::env::sys::env_getter();
    env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
    env.get_or(cert, "FNET_PUBLIC_KEY", "cert.pem");
}
namespace http3 = futils::fnet::http3;
namespace tls = futils::fnet::tls;
using QpackConfig = futils::qpack::TypeConfig<>;
using H3Config = http3::TypeConfig<QpackConfig, DefaultStreamTypeConfig>;
using Conn = http3::stream::Connection<H3Config>;

struct Data {
    std::condition_variable cv;
    std::mutex mtx;
    std::shared_ptr<Context> ctx;
    std::shared_ptr<Conn> conn;
};

void handler_thread(Data* data) {
    {
        std::unique_lock lk(data->mtx);
        data->cv.wait(lk, [&] {
            return data->ctx->handshake_confirmed() || data->ctx->is_closed();
        });
    }

    auto s = data->ctx->get_streams();
    auto ctrl = s->open_uni();
    if (!ctrl) {
        return;
    }

    http3::unistream::UniStreamHeaderArea hdr;
    ctrl->add_data(http3::unistream::get_header(hdr, http3::unistream::Type::CONTROL));
    auto settings = http3::frame::get_header(hdr, http3::frame::Type::SETTINGS, 0);
    ctrl->add_data(settings);
    auto req = s->open_bidi();
    if (!req) {
        return;
    }
    auto r = req->receiver.get_receiver_ctx();
    http3::frame::FrameHeaderArea req_hdr;
    http3::stream::RequestStream<H3Config> reqs;
    reqs.stream = req;
    reqs.conn = data->conn;
    reqs.reader = r;
    reqs.write_header([](auto&&, auto&& add_field) {
        add_field(":method", "GET");
        add_field(":scheme", "https");
        add_field(":authority", "www.google.com");
        add_field(":path", "/");
        add_field("user-agent", "fnet-quic");
    });
    std::multimap<std::string, std::string> headers;
    while (!req->is_closed()) {
        if (reqs.read_header([&](futils::qpack::DecodeField<std::string>& field) {
                headers.emplace(field.key, field.value);
            })) {
            break;
        }
        std::this_thread::yield();
    }
    while (!req->is_closed()) {
        futils::fnet::flex_storage s;
        reqs.read([&](futils::view::rvec data) {
            s.append(data);
        });
        if (s.size()) {
            futils::wrap::cout_wrap() << s << "\n";
        }
    }
}

void log_packet(std::shared_ptr<void>&, futils::fnet::quic::path::PathID, futils::fnet::quic::packet::PacketSummary su, futils::view::rvec payload, bool is_send) {
    std::string res = is_send ? "send: " : "recv: ";
    res += to_string(su.type);
    res += " ";
    futils::number::to_string(res, su.packet_number.as_uint());
    res += " src:";
    for (auto s : su.srcID) {
        if (s < 0x10) {
            res += '0';
        }
        futils::number::to_string(res, s, 16);
    }
    res += " dst:";
    for (auto d : su.dstID) {
        if (d < 0x10) {
            res += '0';
        }
        futils::number::to_string(res, d, 16);
    }
    futils::wrap::cout_wrap() << res << "\n";
    futils::binary::reader r{payload};
    futils::fnet::quic::frame::parse_frames<std::vector>(r, [&](const auto& frame, bool) {
        std::string data;
        futils::fnet::quic::log::format_frame<std::vector>(data, frame, true, false);
        data += "\n";
        if constexpr (std::is_same_v<const futils::fnet::quic::frame::CryptoFrame&, decltype(frame)>) {
            futils::fnet::quic::log::format_crypto(data, frame.crypto_data, false, false);
        }
        futils::wrap::cout_wrap() << data;
    });
}
futils::fnet::quic::status::LossTimerState prev_state = futils::fnet::quic::status::LossTimerState::no_timer;
futils::fnet::quic::time::Time prev_deadline = {};
void record_timer(std::shared_ptr<void>&, const futils::fnet::quic::status::LossTimer& timer, futils::fnet::quic::time::Time now) {
    auto print = [&] {
#ifdef HAS_FORMAT
        // #warning "what?"
        futils::wrap::cout_wrap() << std::format("loss timer: {} {} until deadline {}ms\n", to_string(timer.current_space()), to_string(prev_state), prev_deadline.to_int() - now.to_int());
#else
        futils::wrap::cout_wrap() << "loss timer: " << to_string(timer.current_space()) << " " << to_string(prev_state) << " until deadline " << (prev_deadline.to_int() - now.to_int()) << "ms\n";
#endif
    };
    if (timer.current_state() != prev_state ||
        timer.get_deadline() != prev_deadline) {
        prev_state = timer.current_state();
        prev_deadline = timer.get_deadline();
        if (prev_state != futils::fnet::quic::status::LossTimerState::no_timer) {
            print();
        }
        else {
            futils::wrap::cout_wrap() << "loss timer: canceled\n";
        }
    }
}
void rtt_event(std::shared_ptr<void>&, const futils::fnet::quic::status::RTT& rtt, futils::fnet::quic::time::Time now) {
    futils::wrap::cout_wrap() << futils::wrap::packln("current rtt: ", rtt.smoothed_rtt(), "ms (σ:", rtt.rttvar(), "ms)", " latest rtt: ", rtt.latest_rtt(), "ms");
}

futils::fnet::quic::log::ConnLogCallbacks cbs{
    .drop_packet = [](std::shared_ptr<void>&, futils::fnet::quic::PacketType, futils::fnet::quic::packetnum::Value, futils::fnet::error::Error err, futils::view::rvec packet, bool decrypted) { futils::wrap::cout_wrap() << futils::wrap::packln("drop packet: ", err.error<std::string>()); },
    .debug = [](std::shared_ptr<void>&, const char* msg) { futils::wrap::cout_wrap() << msg << "\n"; },
    .sending_packet = log_packet,
    .recv_packet = log_packet,
    .loss_timer_state = record_timer,
    .mtu_probe = [](std::shared_ptr<void>&, std::uint64_t size) { futils::wrap::cout_wrap() << "mtu probe: " << size << " byte\n"; },
    .rtt_state = rtt_event,
};

int main() {
    load_env();
    tls::set_libcrypto(libcrypto.data());
    tls::set_libssl(libssl.data());
    auto conf = tls::configure();
    conf.set_alpn("\x02h3");
    conf.set_cacert_file(cert.data());
    futils::fnet::quic::log::ConnLogger logger{.callbacks = &cbs};
    auto ctx = make_quic(
        std::move(conf), [](auto&&) {});
    assert(ctx);
    auto res = futils::fnet::connect("www.google.com", "443", futils::fnet::sockattr_udp(), false);
    assert(res);

    auto sock = std::move(res->first);
    auto to = std::move(res->second);
    auto ok = ctx->connect_start("www.google.com");
    assert(ok);

    Data d;
    d.ctx = ctx;
    d.conn = std::make_shared<Conn>();
    std::thread th([&] {
        handler_thread(&d);
    });

    while (true) {
        auto [payload, _, idle] = ctx->create_udp_payload();
        if (!idle) {
            break;
        }
        if (payload.size()) {
            sock.writeto(to.addr, payload);
        }
        futils::byte data[1500];
        auto recv = sock.readfrom(data);
        if (!recv) {
            if (!futils::fnet::isSysBlock(recv.error())) {
                ctx->request_close(recv.error());
            }
        }
        if (recv && recv->first.size()) {
            ctx->parse_udp_payload(recv->first);
        }
        if (ctx->handshake_confirmed()) {
            d.cv.notify_all();
        }

        std::this_thread::yield();
    }
    d.cv.notify_all();
    th.join();

    return 0;
}
