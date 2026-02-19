/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/quic.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <env/env_sys.h>
#include <fnet/connect.h>
using namespace futils::fnet::quic::use::rawptr;
namespace tls = futils::fnet::tls;
namespace quic = futils::fnet::quic;
namespace env_sys = futils::env::sys;

using path = futils::wrap::path_string;
path libssl;
path libcrypto;
std::string cert;

void load_env() {
    auto env = env_sys::env_getter();
    env.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    env.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
    env.get_or(cert, "FNET_PUBLIC_KEY", "cert.pem");
}

int main() {
    load_env();
#ifdef _WIN32
    futils::fnet::tls::set_libcrypto(libcrypto.data());
    futils::fnet::tls::set_libssl(libssl.data());
#endif

    auto [sock, to] = futils::fnet::connect("localhost", "8090", futils::fnet::sockattr_udp(), false).value();

    auto ctx = std::make_shared<Context>();
    auto conf = futils::fnet::tls::configure();
    assert(conf);
    conf.set_early_data_enabled(true);
    conf.set_alpn("\x04test");
    conf.set_cacert_file(cert.data());
    futils::fnet::tls::Session sess;
    conf.set_session_callback([](futils::fnet::tls::Session&& sess, void* cb) {
        *(futils::fnet::tls::Session*)cb = sess;
        return true;
    },
                              &sess);
    auto def = use_default_config(std::move(conf));
    bool reqested = false;
    futils::fnet::flex_storage s;
    auto set_zero_rtt_obj = [&] {
        def.zero_rtt.obj = std::shared_ptr<void>(&s, [](auto) {});
    };
    set_zero_rtt_obj();
    def.zero_rtt.store_token = [](void* p, futils::view::rvec key, quic::token::Token tok) {
        auto& st = *((futils::fnet::flex_storage*)p);
        st = tok.token;
    };
    def.zero_rtt.find_token = [](void* p, futils::view::rvec key) {
        auto& st = *((futils::fnet::flex_storage*)p);
        if (st.size() != 0) {
            return quic::token::TokenStorage{futils::fnet::make_storage(st.rvec())};
        }
        return quic::token::TokenStorage{};
    };
    auto ok = ctx->init(std::move(def)) &&
              ctx->connect_start("localhost");
    assert(ok);
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
        if (recv && recv->first.size()) {
            ctx->parse_udp_payload(recv->first);
        }
        if (s.size() && !reqested) {
            ctx->request_close(AppError(0, "OK"));
        }
    }
    set_zero_rtt_obj();
    def.session = sess;
    auto res = ctx->init(std::move(def));
    ctx->connect_start("localhost");
    auto streams = ctx->get_streams();
    auto stream = streams->open_uni();
    std::shared_ptr<Reader> read;
    auto ch = streams->get_conn_handler();
    ch->arg = &read;
    ch->auto_incr.auto_increase_max_data = true;
    ch->uni_accept_cb = [](void*& p, std::shared_ptr<RecvStream> r) {
        *static_cast<std::shared_ptr<Reader>*>(p) = set_stream_reader(*r, true);
    };
    /*
    streams->set_accept_uni(&read, [](void*& p, std::shared_ptr<RecvStream> r) {
        *static_cast<std::shared_ptr<Reader>*>(p) = set_stream_reader(*r, true);
    });
    */
    assert(stream);
    stream->write("GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n", true);
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
        if (recv && recv->first.size()) {
            ctx->parse_udp_payload(recv->first);
        }
        if (read) {
            while (true) {
                auto [direct, eof] = read->read_direct();
                if (eof) {
                    ctx->request_close(AppError(0, "ok"));
                    read = nullptr;
                    break;
                }
                if (direct.size() == 0) {
                    break;
                }
            }
        }
    }
}
