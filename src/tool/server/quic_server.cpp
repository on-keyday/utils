/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
    auto hm = std::make_shared<HandlerMap>();
    auto tls_conf_or_err = futils::fnet::tls::configure_with_error();
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
    futils::fnet::quic::server::ServerConfig<futils::fnet::quic::use::smartptr::DefaultTypeConfig> serv_conf;
    serv_conf.prepare_connection = trace_id;
    futils::fnet::server::http3_setup(serv_conf);
    state->add_quic_thread(
        std::move(*sock), hm, std::move(conf),
        serv_conf);
    return true;
}

// bool use_log = false;
/*
struct ServerContext {
    std::shared_ptr<HandlerMap> hm;
    fnet::Socket sock;
    std::atomic_uint ref;
    std::atomic_bool end;
    fnet::DeferredCallback cb;
};

struct H3RequestStreamWithHeader {
    H3RequestStream stream;
    std::string method;
    std::string host;
    std::string path;
    std::vector<std::pair<std::string, std::string>> headers;
    fnet::flex_storage body;
};

void recv_packets(std::shared_ptr<ServerContext> ctx) {
    auto initiate_read = [ctx]() {
        while (true) {
            auto result = ctx->sock.readfrom_async_deferred(fnet::async_notify_addr_then(
                fnet::BufferManager<std::array<std::uint8_t, 3000>>{},
                [ctx](fnet::DeferredCallback&& cb) {
                    ctx->cb = std::move(cb);
                },
                [ctx](fnet::Socket&& sock, fnet::BufferManager<std::array<std::uint8_t, 3000>>& mgr, fnet::NetAddrPort&& addr, fnet::NotifyResult&& r) {
                    auto& buffer = mgr.get_buffer();
                    auto expected = r.readfrom_unwrap(mgr.get_buffer(), addr, [&](auto& buffer) {
                        return sock.readfrom(buffer);
                    });
                    if (!expected) {
                        futils::wrap::cout_wrap() << futils::wrap::pack("Failed to receive UDP datagram: ", expected.error().error(), "\n");
                        return;
                    }
                    if (use_log) {
                        futils::wrap::cout_wrap() << futils::wrap::pack("Received UDP datagram from ", expected->second.to_string<std::string>(), "\n");
                    }
                    ctx->hm->parse_udp_payload(expected->first, expected->second);

                    sock.readfrom_until_block(buffer, [&](futils::view::wvec data, fnet::NetAddrPort addr) {
                        if (use_log) {
                            futils::wrap::cout_wrap() << futils::wrap::pack("Received UDP datagram from ", addr.to_string<std::string>(), "\n");
                        }
                        ctx->hm->parse_udp_payload(data, addr);
                    });
                }));
            assert(ctx->sock);
            if (!result) {
                futils::wrap::cout_wrap() << futils::wrap::pack("Failed to receive UDP datagram: ", result.error().error(), "\n");
                continue;
            }
            break;
        }
    };
    initiate_read();
    while (!ctx->end) {
        if (use_log) {
            futils::wrap::cout_wrap() << "Waiting for UDP datagram\n";
        }
        auto res = fnet::wait_io_event(-1);
        if (!res) {
            futils::wrap::cout_wrap() << futils::wrap::pack("Failed to wait for UDP datagram: ", res.error().error(), "\n");
            continue;
        }
        if (ctx->cb) {
            ctx->cb.invoke();
            initiate_read();
        }
        else {
            futils::wrap::cout_wrap() << "Warning: suspend without callback\n";
        }
    }
}

void schedule_send(std::shared_ptr<ServerContext> ctx) {
    while (!ctx->end) {
        ctx->hm->schedule_send();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void schedule_recv(std::shared_ptr<ServerContext> ctx) {
    while (!ctx->end) {
        ctx->hm->schedule_recv();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void send_packets(std::shared_ptr<ServerContext> ctx) {
    fnet::flex_storage buffer;
    while (!ctx->end) {
        auto [data, addr, exist] = ctx->hm->create_packet(buffer);
        if (!exist) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (data.size()) {
            ctx->sock.writeto(addr, data);
        }
    }
}

std::shared_ptr<H3Conn> get_h3(const std::shared_ptr<Connection>& ctx) {
    return std::static_pointer_cast<H3Conn>(ctx->get_application_context());
}
*/

/*
void connection_init(std::shared_ptr<void>& app, std::shared_ptr<Connection>&& ctx) {
    auto conn = std::make_shared<H3Conn>();
    ctx->set_application_context(conn);
    auto streams = ctx->get_streams();
    auto control = streams->open_uni();
    auto qpack_encoder = streams->open_uni();
    auto qpack_decoder = streams->open_uni();
    conn->ctrl.send_control = control;
    conn->send_decoder = qpack_decoder;
    conn->send_encoder = qpack_encoder;
    conn->write_uni_stream_headers_and_settings([&](auto&& write_settings) {});
}


void connection_close(std::shared_ptr<void>& app, std::shared_ptr<Connection>&& ctx) {
    auto conn = get_h3(ctx);
    auto streams = ctx->get_streams();
    streams->iterate_bidi_stream([&](std::shared_ptr<quic::use::smartptr::BidiStream>& stream) {
        auto req = std::static_pointer_cast<H3RequestStreamWithHeader>(stream->receiver.get_application_context());
        if (!req) {
            return;
        }
        auto out = futils::wrap::pack();
        stream->sender.detect_ack_lost();
        if (req->method.empty() || req->path.empty()) {
            return;
        }
        out << futils::number::hex::to_hex<std::string>(ctx->get_trace_id()) << " ";
        out << "Stream " << stream->id().as_uint() << "\n";
        out << req->method << " " << req->path << " HTTP/3" << "\n";
        for (auto& [key, value] : req->headers) {
            out << key << ": " << value << "\n";
        }
        out << "[canclled]\n";
        cout << out.pack();
    });
}


void on_read_uni_stream(std::shared_ptr<void>& app, std::shared_ptr<Connection>&& ctx, std::shared_ptr<quic::use::smartptr::RecvStream>&& stream) {
    auto h3 = get_h3(ctx);
    h3->read_uni_stream(std::move(stream));
}

void on_read_bidi_stream(std::shared_ptr<void>& app, std::shared_ptr<Connection>&& ctx, std::shared_ptr<quic::use::smartptr::BidiStream>&& stream) {
    auto h3 = get_h3(ctx);
    assert(h3);
    auto req = std::static_pointer_cast<H3RequestStreamWithHeader>(stream->receiver.get_or_set_application_context([&](std::shared_ptr<void>&& ctx, auto&& setter) {
        if (ctx) {
            return ctx;
        }
        auto req = std::make_shared<H3RequestStreamWithHeader>();
        req->stream.conn = h3;
        req->stream.stream = stream;
        req->stream.state = http3::stream::StateMachine::server_start;
        setter(req);
        return std::static_pointer_cast<void>(req);
    }));
    req->stream.read_header([&](qpack::DecodeField<std::string>& r) {
        if (r.key == ":method") {
            req->method = r.value;
        }
        else if (r.key == ":path") {
            req->path = r.value;
        }
        else if (r.key == ":authority") {
            req->host = r.value;
        }
        else {
            req->headers.push_back({r.key, r.value});
        }
    });
    req->stream.read([&](futils::view::rvec data) {
        req->body.append(data);
    });
    auto text = futils::view::rvec("hello world");
    req->stream.write_header(false, [&](auto&& add_entry, auto&& add_field) {
        add_field(":status", "200");
        add_field("content-type", "text/plain");
        add_field("content-length", futils::number::to_string<std::string>(text.size()));
    });
    req->stream.write([&](auto&& write) {
        write(text, true);
        return true;
    });
    if (req->stream.state == http3::stream::StateMachine::server_end ||
        req->stream.state == http3::stream::StateMachine::failed) {
        auto out = futils::wrap::pack();
        out << futils::number::hex::to_hex<std::string>(ctx->get_trace_id()) << " ";
        out << "Stream " << stream->id().as_uint() << "\n";
        out << req->method << " " << req->path << " HTTP/3" << "\n";
        out << "Host: " << req->host << "\n";
        for (auto& [key, value] : req->headers) {
            out << key << ": " << value << "\n";
        }
        out << "[finished]\n";
        cout << out.pack();
        stream->receiver.user_read_full();
    }
}
*/

/*
using path = futils::wrap::path_string;
path libssl;
path libcrypto;
bool bind_public = false;

#ifdef _WIN32
#define SUFFIX ".dll"
#else
#define SUFFIX ".so"
#endif

void load_env(std::string& cert, std::string& prv) {
    auto env = futils::env::sys::env_getter();
    env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl" SUFFIX));
    env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto" SUFFIX));
    env.get_or(cert, "FNET_PUBLIC_KEY", "cert.pem");
    env.get_or(prv, "FNET_PRIVATE_KEY", "private.pem");
    use_log = env.get<std::string>("FNET_QUIC_LOG") == "1";
    bind_public = env.get<std::string>("FNET_BIND_PUBLIC") == "1";
    futils::fnet::tls::set_libcrypto(libcrypto.data());
    futils::fnet::tls::set_libssl(libssl.data());
}
*/

/*
int quic_server(Flags& flag) {
    use_log = flag.verbose;
    auto& cout = futils::wrap::cout_wrap();
    auto ctx = std::make_shared<ServerContext>();
    ctx->hm = std::make_shared<HandlerMap>();
    auto tls_conf_or_err = futils::fnet::tls::configure_with_error();
    if (!tls_conf_or_err) {
        cout << "failed to configure tls " << tls_conf_or_err.error().error() << "\n";
        return 1;
    }
    auto tls_conf = tls_conf_or_err.value();
    if (auto ok = tls_conf.set_cert_chain(flag.public_key.c_str(), flag.private_key.c_str()); !ok) {
        cout << "failed to load cert " << ok.error().error() << "\n";
        return 1;
    }
    futils::fnet::tls::ALPNCallback cb;
    cb.select = [](futils::fnet::tls::ALPNSelector& selector, void*) {
        futils::view::rvec name;
        while (selector.next(name)) {
            if (name == "h3") {
                selector.select(name);
                return true;
            }
        }
        return false;
    };
    tls_conf.set_alpn_select_callback(&cb);
    auto conf = futils::fnet::quic::use::use_default_config(std::move(tls_conf));
    if (use_log) {
        conf.logger.callbacks = &log_cb;
        conf.logger.ctx = ctx;
    }
    ctx->hm->set_config(
        std::move(conf),
        {
            .app_ctx = ctx,
            .prepare_connection = trace_id,
            .init_recv_stream = [](std::shared_ptr<void>&, futils::fnet::quic::use::smartptr::RecvStream& stream) { futils::fnet::quic::stream::impl::set_stream_reader(stream, true, HandlerMap::stream_recv_notify_callback); },
            .init_send_stream = [](std::shared_ptr<void>&, futils::fnet::quic::use::smartptr::SendStream& stream) { futils::fnet::quic::stream::impl::set_on_send_callback(stream, HandlerMap::stream_send_notify_callback); },
            .on_transport_parameter_read = connection_init,
            .close_connection = connection_close,
            .on_bidi_stream_recv = on_read_bidi_stream,
            .on_uni_stream_recv = on_read_uni_stream,
        });
    auto list = fnet::get_self_server_address(flag.port, fnet::sockattr_udp(fnet::ip::Version::ipv4), flag.bind_public ? futils::view::rvec{} : "localhost")
                    .and_then([&](fnet::WaitAddrInfo&& info) {
                        return info.wait();
                    });
    if (!list) {
        cout << "failed to get server address " << list.error().error() << "\n";
        return 1;
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
            break;
        }
    }
    if (!sock) {
        cout << "failed to create socket " << sock.error().error() << "\n";
        return 1;
    }
    ctx->sock = std::move(*sock);
    ctx->sock.set_DF(true);
    std::thread(recv_packets, ctx).detach();
    std::thread(send_packets, ctx).detach();
    std::thread(schedule_send, ctx).detach();
    std::thread(schedule_recv, ctx).detach();
    if (use_log) {
        cout << "verbose log enabled\n";
    }
    cout << futils::wrap::pack("quic server listening on ", flag.port, "\n");
    while (!ctx->end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    return 0;
}
*/
