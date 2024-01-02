/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include <fnet/quic/path/dlpmtud.h>
#ifdef _WIN32
#include <format>
#define HAS_FORMAT
#endif
#include <thread/channel.h>
#include <fnet/quic/stream/impl/recv_stream.h>
#include <fnet/http.h>
#include <fstream>
#include <file/gzip/gzip.h>
#include <testutil/timer.h>
#include <fnet/quic/quic.h>
#include <fnet/connect.h>
#include <env/env_sys.h>

using namespace futils::fnet::quic::use::smartptr;
using QCTX = std::shared_ptr<Context>;
using namespace futils::fnet;

struct Recvs {
    std::shared_ptr<RecvStream> s;
    std::shared_ptr<Reader> r;
};

using SendChan = futils::thread::SendChan<Recvs>;
using RecvChan = futils::thread::RecvChan<Recvs>;

std::atomic_uint ref = 0;

void thread(QCTX ctx, RecvChan c, int i) {
    auto decref = futils::helper::defer([&] {
        ref--;
    });
    size_t data_size = 0;
    const auto def = futils::helper::defer([&] {
        futils::wrap::cout_wrap() << futils::wrap::packln("thread ", i, " finished recv:", data_size);
    });

    auto streams = ctx->get_streams();
    std::shared_ptr<SendStream> uni;
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
#ifdef HAS_FORMAT
    auto res = uni->add_data(std::format("GET /get HTTP/1.1\r\nHost: www.google.com\r\n", i + 1), false);
#else
    std::string req = "GET /search?q=" + futils::number::to_string<std::string>(i + 1) + " HTTP/1.1\r\nHost: www.google.com\r\n";
    auto res = uni->add_data(req, false);
#endif
    assert(res.result == futils::fnet::quic::IOResult::ok);
    res = uni->add_data("Accept-Encoding: gzip\r\n", false);
    assert(res.result == futils::fnet::quic::IOResult::ok);
    res = uni->add_data("\r\n", true, true);
    assert(res.result == futils::fnet::quic::IOResult::ok);
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
    futils::wrap::cout_wrap() << futils::wrap::packln("uni stream sent: ", i, "->", sent_id);
    res.result = streams->remove(uni->id());
    assert(res.result == futils::fnet::quic::IOResult::ok);
    Recvs runi;
    while (!runi.s) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        c >> runi;
    }
    futils::fnet::HTTP http;
    while (true) {
        runi.s->update_recv_limit([](FlowLimiter limit, std::uint64_t) {
            if (limit.avail_size() < 10000) {
                return limit.current_limit() + 10000;
            }
            return limit.current_limit();  // not update
        });

        bool is_eos = false;
        while (true) {
            auto [red, eos] = runi.r->read_direct();
            is_eos = eos;
            if (red.size() == 0) {
                break;
            }
            http.add_input(red);
            if (eos) {
                break;
            }
        }
        if (is_eos) {
            break;
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    res.result = streams->remove(runi.s->id());
    // decref.execute();

    assert(res.result == futils::fnet::quic::IOResult::ok);
    futils::http::header::StatusCode code;
    std::unordered_multimap<std::string, std::string> resp;
    futils::fnet::HTTPBodyInfo info;
    http.read_response<std::string>(code, resp, &info);
    std::string text;
    bool inval = false;
    auto v = http.read_body(text, info, 0, 0);
    assert(v == futils::http::body::BodyReadResult::full);
    http.write_response(code, resp);
    futils::wrap::cout_wrap() << http.get_output();

    auto path = "./ignore/dump" + futils::number::to_string<std::string>(i) + ".";
    {
        std::ofstream of(path + "gz", std::ios_base::out | std::ios_base::binary);
        of << text;
    }
    {
        std::ofstream of(path + "html", std::ios_base::out | std::ios_base::binary);
        futils::file::gzip::GZipHeader gh;
        std::string dec;
        futils::binary::bit_reader<std::string&> in{text};
        futils::binary::reader tmpr{futils::view::rvec(text).substr(0, text.size() - 4)};
        std::uint32_t reserve = 0;
        futils::binary::read_num(tmpr, reserve, false);
        dec.reserve(reserve);
        futils::file::gzip::decode_gzip(dec, gh, in);
        data_size = dec.size();
        of << dec;
    }
}

void log_packet(std::shared_ptr<void>&, futils::fnet::quic::packet::PacketSummary su, futils::view::rvec payload, bool is_send) {
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
    futils::fnet::quic::frame::parse_frames<slib::vector>(r, [&](const auto& frame, bool) {
        std::string data;
        futils::fnet::quic::log::format_frame<slib::vector>(data, frame, true, false);
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

void load_env(std::string& cert) {
    auto env = futils::env::sys::env_getter();
    auto ssl = env.get_or<futils::wrap::path_string>("FNET_LIBSSL", "ssl.dll");
    auto crypto = env.get_or<futils::wrap::path_string>("FNET_LIBCRYPTO", "crypto.dll");
    tls::set_libcrypto(crypto.c_str());
    tls::set_libssl(ssl.c_str());
    tls::load_crypto();
    tls::load_ssl();
    cert = env.get_or<std::string>("FNET_PUBLIC_KEY", "D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
}

int main() {
    futils::fnet::set_normal_allocs(futils::fnet::debug::allocs());
    futils::fnet::set_objpool_allocs(futils::fnet::debug::allocs());
    futils::test::log_hooker = [](futils::test::HookInfo info) {
        if (info.size == 232) {
        }
    };
    futils::test::set_alloc_hook(true);
    futils::test::set_log_file("memuse.log");
    std::string cert;
    load_env(cert);
    QCTX ctx = std::make_shared<Context>();
    tls::TLSConfig conf = tls::configure_with_error().value();
    conf.set_cacert_file(cert.c_str());
    conf.set_alpn("\4test");
    auto config = futils::fnet::quic::context::use_default_config(std::move(conf));
    config.logger.callbacks = &cbs;
    config.connid_parameters.random = futils::fnet::quic::connid::Random{
        nullptr,
        [](std::shared_ptr<void>& arg, futils::view::wvec rand, futils::fnet::quic::connid::RandomUsage u) {
            if (u == futils::fnet::quic::connid::RandomUsage::original_dst_id) {
                rand[0] = 0x2f;
                rand[1] = 0x00;
                rand = rand.substr(2);
            }
            return futils::fnet::quic::context::default_gen_random(arg, rand, u);
        },
    };
    auto val = ctx->init(std::move(config)) &&
               ctx->connect_start();
    assert(val);

    auto streams = ctx->get_streams();
    auto [w, r] = futils::thread::make_chan<Recvs>();
    auto ch = streams->get_conn_handler();
    ch->arg = std::shared_ptr<std::decay_t<decltype(w)>>(&w, [](auto*) {});
    ch->uni_accept_cb = [](std::shared_ptr<void>& p, std::shared_ptr<RecvStream> s) {
        auto w2 = static_cast<SendChan*>(p.get());
        auto data = set_stream_reader(*s);
        *w2 << Recvs{std::move(s), std::move(data)};
    };
    /*
    streams->set_accept_uni(, [](std::shared_ptr<void>& p, std::shared_ptr<RecvStream> s) {
        auto w2 = static_cast<SendChan*>(p.get());
        auto data = set_stream_reader(*s);
        *w2 << Recvs{std::move(s), std::move(data)};
    });
    */

    auto [sock, addr] = connect("localhost", "8090", sockattr_udp(), false).value();

    assert(sock);
    constexpr auto tasks = 11;
    for (auto i = 0; i < tasks; i++) {
        ref++;
        std::thread(thread, std::as_const(ctx), r, i).detach();
    }
    futils::view::rvec data;
    futils::byte buf[3000];
    futils::test::Timer timer;
    while (true) {
        std::tie(data, std::ignore, val) = ctx->create_udp_payload();
        if (!val && ctx->is_closed()) {
            futils::wrap::cout_wrap() << futils::wrap::packln(ctx->get_conn_error().error<std::string>());
            break;
        }
        if (!val) {
            futils::wrap::cout_wrap() << ctx->get_conn_error().error<std::string>() << "\n";
            assert(val);
        }
        if (data.size()) {
            sock.writeto(addr.addr, data);
        }
        while (true) {
            auto data = sock.readfrom(buf);
            if (!data && isSysBlock(data.error())) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                break;
            }
            if (!data) {
                break;
            }
            if (ctx->parse_udp_payload(data->first)) {
                break;
            }
        }
        if (ref == 0) {
            ctx->request_close(quic::QUICError{
                .msg = "task completed",
                .transport_error = quic::TransportError::NO_ERROR,
            });
        }
        streams->update_max_data([](FlowLimiter limit, std::uint64_t) {
            if (limit.avail_size() < 10000) {
                return limit.current_limit() + 50000;
            }
            return limit.current_limit();
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    futils::wrap::cout_wrap() << timer.delta().count();
}
