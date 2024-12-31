/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <wrap/cout.h>
#include <fnet/quic/server/server.h>
#include <fnet/quic/quic.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <thread>
#include <wrap/cout.h>
#include <env/env_sys.h>
#include <number/hex/bin2hex.h>
#include <fnet/http3/typeconfig.h>
#include <fnet/http3/stream.h>
#include <fnet/util/qpack/typeconfig.h>
#include <array>
#include <fnet/util/uri.h>
#include "flags.h"
#include <fnet/server/state.h>
#include <fnet/server/httpserv.h>
#include <fnet/quic/log/frame_log.h>

namespace fnet = futils::fnet;
namespace quic = futils::fnet::quic;
namespace http3 = futils::fnet::http3;
namespace qpack = futils::qpack;
using H3Conf = http3::TypeConfig<qpack::TypeConfig<>, quic::use::smartptr::DefaultStreamTypeConfig>;
using H3Conn = http3::stream::Connection<H3Conf>;
using H3RequestStream = http3::stream::RequestStream<H3Conf>;
using HandlerMap = quic::server::HandlerMap<quic::use::smartptr::DefaultTypeConfig>;
using Connection = quic::use::smartptr::Context;
extern futils::wrap::UtfOut& cout;

void trace_id(std::shared_ptr<void>& app, std::shared_ptr<Connection>&& ctx) {
    // pointer to connection is unique per connection so we can use it as trace id
    auto id = ctx.get();
    ctx->set_trace_id(futils::view::rvec((char*)&id, sizeof(id)));
    auto streams = ctx->get_streams();
    streams->set_auto_remove(true);
    streams->get_conn_handler()->set_auto_increase(false, true, true);
}

void pkt(std::shared_ptr<void>& ctx_, futils::view::rvec traceID, futils::fnet::quic::path::PathID path, futils::fnet::quic::packet::PacketSummary summary, futils::view::rvec payload, bool is_send) {
    auto hm = std::static_pointer_cast<HandlerMap>(ctx_);
    auto addr = hm->get_peer_addr(path);
    auto addr_str = addr.to_string<std::string>();
    auto dstID = futils::number::hex::to_hex<std::string>(summary.dstID);
    auto srcID = futils::number::hex::to_hex<std::string>(summary.srcID);
    auto type = to_string(summary.type);
    auto pn = futils::number::to_string<std::string>(summary.packet_number.as_uint());
    auto out = futils::wrap::pack();
    auto trace = futils::number::hex::to_hex<std::string>(traceID);
    out << trace << " " << (is_send ? "send " : "recv ") << addr_str << " " << type << " " << pn << " srcID: " << srcID << " dstID: " << dstID << " payload size:" << payload.size() << "\n";
    std::string frame_log;
    futils::binary::reader r{payload};
    futils::fnet::quic::frame::parse_frames<std::vector>(r, [&](auto&& f, bool err) {
        if (err) {
            out << "failed to parse frame\n";
        }
        else {
            futils::fnet::quic::log::format_frame<std::vector>(frame_log, f, false, false);
        }
    });
    out << frame_log;
    log(out.pack());
}

futils::fnet::quic::log::ConnLogCallbacks log_cb{
    .debug = [](std::shared_ptr<void>&, futils::view::rvec traceID, const char* str) { log(futils::wrap::pack(futils::number::hex::to_hex<std::string>(traceID), " ", str, "\n")); },
    .report_error = [](std::shared_ptr<void>&, futils::view::rvec traceID, const futils::fnet::error::Error& err) { log(futils::wrap::pack(futils::number::hex::to_hex<std::string>(traceID), " ", err.error(), "\n")); },
    .sending_packet = pkt,
    .recv_packet = pkt,
};

bool setup_http3_server(Flags& flag, std::shared_ptr<futils::fnet::server::State>& state, std::string& out_address) {
    if (!futils::fnet::quic::crypto::has_quic_ext()) {
        cout << "quic extension not available for libssl\n";
        return false;
    }
    auto hm = std::make_shared<HandlerMap>();
    auto tls_conf_or_err = futils::fnet::tls::configure_with_error();
    if (!tls_conf_or_err) {
        cout << "failed to configure tls " << tls_conf_or_err.error().error() << "\n";
        return false;
    }
    auto tls_conf = tls_conf_or_err.value();
    if (auto ok = tls_conf.set_cert_chain(flag.public_key.c_str(), flag.private_key.c_str()); !ok) {
        cout << "failed to load cert " << ok.error().error() << "\n";
        return false;
    }
    tls_conf.set_alpn_select_callback(
        [](futils::fnet::tls::ALPNSelector& selector, void*) {
            futils::view::rvec name;
            while (selector.next(name)) {
                if (name == "h3") {
                    selector.select(name);
                    return true;
                }
            }
            return false;
        },
        nullptr);

    if (flag.key_log.size()) {
        tls_conf.set_key_log_callback(key_log, nullptr);
    }
    auto conf = futils::fnet::quic::use::use_default_config(std::move(tls_conf));
    if (flag.verbose) {
        conf.logger.callbacks = &log_cb;
        conf.logger.ctx = hm;
    }
    auto list = fnet::get_self_server_address(flag.quic_port, fnet::sockattr_udp(fnet::ip::Version::unspec), flag.bind_public ? futils::view::rvec{} : "localhost")
                    .and_then([&](fnet::WaitAddrInfo&& info) {
                        return info.wait();
                    });
    if (!list) {
        cout << "failed to get server address " << list.error().error() << "\n";
        return false;
    }
    fnet::expected<fnet::Socket> sock;
    while (list->next()) {
        auto addr = list->sockaddr();
        sock = fnet::make_socket(addr.attr)
                   .and_then([&](fnet::Socket&& s) {
                       return s.bind(addr.addr)
                           .transform([&] {
                               return std::move(s);
                           });
                   });

        if (sock) {
            out_address = addr.addr.to_string<std::string>();
            break;
        }
    }
    if (!sock) {
        cout << "failed to create socket " << sock.error().error() << "\n";
        return false;
    }
    futils::fnet::quic::server::MultiplexerConfig<futils::fnet::quic::use::smartptr::DefaultTypeConfig> serv_conf;
    serv_conf.prepare_connection = trace_id;
    futils::fnet::server::http3_server_setup(serv_conf);
    state->add_quic_thread(
        std::move(*sock), hm, std::move(conf),
        serv_conf);
    return true;
}
