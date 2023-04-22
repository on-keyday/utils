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
#include <thread/channel.h>
#include <fnet/quic/stream/impl/recv_stream.h>
#include <format>
#include <fnet/http.h>
#include <fstream>
#include <file/gzip/gzip.h>
using Context = utils::fnet::quic::context::Context<std::mutex>;
using Config = utils::fnet::quic::context::Config;
using QCTX = std::shared_ptr<Context>;
using namespace utils::fnet;
using SendStream = std::shared_ptr<quic::stream::impl::SendUniStream<std::mutex>>;
using RecvStream = std::shared_ptr<quic::stream::impl::RecvUniStream<std::mutex>>;
using Reader = quic::stream::impl::RecvSorted<std::mutex>;

struct Recvs {
    RecvStream s;
    std::shared_ptr<Reader> r;
};

using SendChan = utils::thread::SendChan<Recvs>;
using RecvChan = utils::thread::RecvChan<Recvs>;

std::atomic_uint ref = 0;

void thread(QCTX ctx, RecvChan c, int i) {
    auto decref = utils::helper::defer([&] {
        ref--;
    });
    size_t data_size = 0;
    const auto def = utils::helper::defer([&] {
        utils::wrap::cout_wrap() << utils::wrap::packln("thread ", i, " finished recv:", data_size);
    });

    auto streams = ctx->get_streams();
    SendStream uni;
    while (true) {
        if (ctx->is_closed()) {
            return;
        }
        uni = streams->open_uni();
        if (!uni) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        break;
    }
    uni->add_data(std::format("GET /search?q={} HTTP/1.1\r\nHost: www.google.com\r\n", i + 1), false);
    uni->add_data("Accept-Encoding: gzip\r\n", false);
    uni->add_data("\r\n", true);
    while (!uni->is_closed()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (ctx->is_closed()) {
            return;
        }
    }
    if (ctx->is_closed()) {
        return;
    }
    auto sent_id = uni->id();
    utils::wrap::cout_wrap() << utils::wrap::packln("uni stream sent: ", i, "->", sent_id);
    auto res = streams->remove(uni->id());
    assert(res == utils::fnet::quic::IOResult::ok);
    Recvs runi;
    while (!runi.s) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        c >> runi;
    }
    utils::fnet::HTTP http;
    while (true) {
        runi.s->update_recv_limit([](quic::stream::Limiter limit) {
            if (limit.avail_size() < 10000) {
                return limit.curlimit() + 10000;
            }
            return limit.curlimit();  // not update
        });
        utils::byte data[1200];
        bool is_eos = false;
        while (true) {
            auto [red, eos] = runi.r->read(data);
            is_eos = eos;
            if (red.size() == 0) {
                break;
            }
            http.add_input(red, red.size());
            if (eos) {
                break;
            }
        }
        if (is_eos) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    res = streams->remove(runi.s->id());
    decref.execute();
    assert(res == utils::fnet::quic::IOResult::ok);
    utils::http::header::StatusCode code;
    std::unordered_multimap<std::string, std::string> resp;
    utils::fnet::HTTPBodyInfo info;
    http.read_response<std::string>(code, resp, &info);
    std::string text;
    bool inval = false;
    auto v = http.read_body(text, info, 0, 0, &inval);
    assert(v);
    http.write_response(code, resp);
    const char* data;
    size_t size;
    http.borrow_output(data, size);
    utils::wrap::cout_wrap() << utils::view::CharVec(data, size);

    {
        std::ofstream of(std::format("./ignore/dump{}.gz", i), std::ios_base::out | std::ios_base::binary);
        of << text;
    }
    {
        std::ofstream of(std::format("./ignore/dump{}.html", i), std::ios_base::out | std::ios_base::binary);
        utils::file::gzip::GZipHeader gh;
        std::string dec;
        utils::io::bit_reader<std::string&> in{text};
        utils::io::reader tmpr{utils::view::rvec(text).substr(0, text.size() - 4)};
        std::uint32_t reserve = 0;
        utils::io::read_num(tmpr, reserve, false);
        dec.reserve(reserve);
        utils::file::gzip::decode_gzip(dec, gh, in);
        data_size = dec.size();
        of << dec;
    }
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
    utils::fnet::quic::frame::parse_frames<slib::vector>(r, [&](const auto& frame, bool) {
        std::string data;
        utils::fnet::quic::log::format_frame<slib::vector>(data, frame, true, false);
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
void rtt_event(std::shared_ptr<void>&, const utils::fnet::quic::status::RTT& rtt, utils::fnet::quic::time::Time now) {
    utils::wrap::cout_wrap() << utils::wrap::packln("current rtt: ", rtt.smoothed_rtt(), "ms (σ:", rtt.rttvar(), "ms)", " latest rtt: ", rtt.latest_rtt(), "ms");
}

utils::fnet::quic::log::LogCallbacks cbs{
    .debug = [](std::shared_ptr<void>&, const char* msg) { utils::wrap::cout_wrap() << msg << "\n"; },
    .sending_packet = log_packet,
    .recv_packet = log_packet,
    .loss_timer_state = record_timer,
    .mtu_probe = [](std::shared_ptr<void>&, std::uint64_t size) { utils::wrap::cout_wrap() << "mtu probe: " << size << " byte\n"; },
    .rtt_state = rtt_event,
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
    config.transport_parameters.initial_max_streams_uni = 100;
    config.transport_parameters.initial_max_data = 1000000;
    config.transport_parameters.initial_max_stream_data_uni = 100000;
    config.internal_parameters.ping_duration = 15000;
    // config.logger.callbacks = &cbs;
    auto val = ctx->init(std::move(config)) &&
               ctx->connect_start();
    assert(val);

    auto streams = ctx->get_streams();
    auto [w, r] = utils::thread::make_chan<Recvs>();
    streams->set_accept_uni(std::shared_ptr<std::decay_t<decltype(w)>>(&w, [](auto*) {}), [](std::shared_ptr<void>& p, RecvStream s) {
        auto w2 = static_cast<SendChan*>(p.get());
        auto data = std::make_shared<Reader>();
        s->set_receiver(data, quic::stream::impl::recv_handler<std::mutex>);
        *w2 << Recvs{std::move(s), std::move(data)};
    });
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
    constexpr auto tasks = 12;
    for (auto i = 0; i < tasks; i++) {
        ref++;
        std::thread(thread, std::as_const(ctx), r, i).detach();
    }
    utils::view::rvec data;
    utils::byte buf[3000];
    while (true) {
        std::tie(data, val) = ctx->create_udp_payload();
        if (!val && ctx->is_closed()) {
            utils::wrap::cout_wrap() << utils::wrap::packln(ctx->get_conn_error().error<std::string>());
            break;
        }
        if (!val) {
            utils::wrap::cout_wrap() << ctx->get_conn_error().error<std::string>() << "\n";
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
            if (err) {
                break;
            }
            if (!ctx->parse_udp_payload(d)) {
                break;
            }
        }
        if (ref == 0) {
            ctx->request_close(quic::QUICError{
                .msg = "task completed",
                .transport_error = quic::TransportError::NO_ERROR,
            });
        }
        streams->update_max_data([](quic::stream::Limiter limit) {
            if (limit.avail_size() < 10000) {
                return limit.curlimit() + 50000;
            }
            return limit.curlimit();
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
