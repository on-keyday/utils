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

using namespace utils::fnet::quic::use::smartptr;
using QCTX = std::shared_ptr<Context>;
using namespace utils::fnet;

struct Recvs {
    std::shared_ptr<RecvStream> s;
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
    std::string req = "GET /search?q=" + utils::number::to_string<std::string>(i + 1) + " HTTP/1.1\r\nHost: www.google.com\r\n";
    auto res = uni->add_data(req, false);
#endif
    assert(res == utils::fnet::quic::IOResult::ok);
    res = uni->add_data("Accept-Encoding: gzip\r\n", false);
    assert(res == utils::fnet::quic::IOResult::ok);
    res = uni->add_data("\r\n", true, true);
    assert(res == utils::fnet::quic::IOResult::ok);
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
    res = streams->remove(uni->id());
    assert(res == utils::fnet::quic::IOResult::ok);
    Recvs runi;
    while (!runi.s) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        c >> runi;
    }
    utils::fnet::HTTP http;
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
    res = streams->remove(runi.s->id());
    // decref.execute();

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
    utils::wrap::cout_wrap() << http.get_output();

    auto path = "./ignore/dump" + utils::number::to_string<std::string>(i) + ".";
    {
        std::ofstream of(path + "gz", std::ios_base::out | std::ios_base::binary);
        of << text;
    }
    {
        std::ofstream of(path + "html", std::ios_base::out | std::ios_base::binary);
        utils::file::gzip::GZipHeader gh;
        std::string dec;
        utils::binary::bit_reader<std::string&> in{text};
        utils::binary::reader tmpr{utils::view::rvec(text).substr(0, text.size() - 4)};
        std::uint32_t reserve = 0;
        utils::binary::read_num(tmpr, reserve, false);
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
    utils::binary::reader r{payload};
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
#ifdef HAS_FORMAT
        // #warning "what?"
        utils::wrap::cout_wrap() << std::format("loss timer: {} {} until deadline {}ms\n", to_string(timer.current_space()), to_string(prev_state), prev_deadline.to_int() - now.to_int());
#else
        utils::wrap::cout_wrap() << "loss timer: " << to_string(timer.current_space()) << " " << to_string(prev_state) << " until deadline " << (prev_deadline.to_int() - now.to_int()) << "ms\n";
#endif
    };
    if (timer.current_state() != prev_state ||
        timer.get_deadline() != prev_deadline) {
        prev_state = timer.current_state();
        prev_deadline = timer.get_deadline();
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

utils::fnet::quic::log::ConnLogCallbacks cbs{
    .drop_packet = [](std::shared_ptr<void>&, utils::fnet::quic::PacketType, utils::fnet::quic::packetnum::Value, utils::fnet::error::Error err, utils::view::rvec packet, bool decrypted) { utils::wrap::cout_wrap() << utils::wrap::packln("drop packet: ", err.error<std::string>()); },
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
    tls::TLSConfig conf = tls::configure();
    conf.set_cacert_file("D:/MiniTools/QUIC_mock/goserver/keys/quic_mock_server.crt");
    conf.set_alpn("\4test");
    auto config = utils::fnet::quic::context::use_default_config(std::move(conf));
    config.logger.callbacks = &cbs;
    auto val = ctx->init(std::move(config)) &&
               ctx->connect_start();
    assert(val);

    auto streams = ctx->get_streams();
    auto [w, r] = utils::thread::make_chan<Recvs>();
    auto ch = streams->conn_handler();
    ch->arg = std::shared_ptr<std::decay_t<decltype(w)>>(&w, [](auto*) {});
    ch->uni_accept_cb = [](std::shared_ptr<void>& p, std::shared_ptr<RecvStream> s) {
        auto w2 = static_cast<SendChan*>(p.get());
        auto data = set_stream_reader(*s);
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
    utils::view::rvec data;
    utils::byte buf[3000];
    utils::test::Timer timer;
    while (true) {
        std::tie(data, std::ignore, val) = ctx->create_udp_payload();
        if (!val && ctx->is_closed()) {
            utils::wrap::cout_wrap() << utils::wrap::packln(ctx->get_conn_error().error<std::string>());
            break;
        }
        if (!val) {
            utils::wrap::cout_wrap() << ctx->get_conn_error().error<std::string>() << "\n";
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
    utils::wrap::cout_wrap() << timer.delta().count();
}
