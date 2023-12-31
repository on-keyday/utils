/*
    utils - utility library
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

using namespace utils::fnet::quic::use::smartptr;

using path = utils::wrap::path_string;
path libssl;
path libcrypto;
std::string cert;

void load_env() {
    auto env = utils::env::sys::env_getter();
    env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
    env.get_or(cert, "FNET_PUBLIC_KEY", "cert.pem");
}
namespace http3 = utils::fnet::http3;
namespace tls = utils::fnet::tls;
using QpackConfig = utils::qpack::TypeConfig<>;
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
        add_field(":authority", "localhost");
        add_field(":path", "/world");
        add_field("user-agent", "fnet-quic");
    });
    std::multimap<std::string, std::string> headers;
    while (!req->is_closed()) {
        if (reqs.read_header([&](utils::qpack::DecodeField<std::string>& field) {
                headers.emplace(field.key, field.value);
            })) {
            break;
        }
        std::this_thread::yield();
    }
    while (!req->is_closed()) {
        utils::fnet::flex_storage s;
        reqs.read([&](utils::view::rvec data) {
            s.append(data);
        });
        if (s.size()) {
            utils::wrap::cout_wrap() << s << "\n";
        }
    }
}

int main() {
    load_env();
    tls::set_libcrypto(libcrypto.data());
    tls::set_libssl(libssl.data());
    auto conf = tls::configure();
    conf.set_alpn("\x02h3");
    conf.set_cacert_file(cert.data());
    auto ctx = make_quic(std::move(conf), [](auto&&) {});
    assert(ctx);
    auto res = utils::fnet::connect("localhost", "443", utils::fnet::sockattr_udp(), false);
    assert(res);

    auto sock = std::move(res->first);
    auto to = std::move(res->second);
    auto ok = ctx->connect_start("localhost");
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
        utils::byte data[1500];
        auto recv = sock.readfrom(data);
        if (!recv) {
            if (!utils::fnet::isSysBlock(recv.error())) {
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
