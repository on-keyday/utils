/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/quic.h>
#include <fnet/connect.h>
#include <env/env_sys.h>
#include <wrap/cout.h>
#include <timer/to_string.h>
#include <timer/clock.h>

void load_root_cert(std::string& root_cert, futils::wrap::path_string& libssl, futils::wrap::path_string& libcrypto) {
    auto e = futils::env::sys::env_getter();
    e.get_or(root_cert, "FNET_ROOT_CA", "root.pem");
    e.get_or(libssl, "FNET_LIBSSL", fnet_lazy_dll_path("libssl.dll"));
    e.get_or(libcrypto, "FNET_LIBCRYPTO", fnet_lazy_dll_path("libcrypto.dll"));
}

auto& cout = futils::wrap::cout_wrap();

int main() {
    namespace fnet = futils::fnet;
    using namespace futils::fnet::quic;
    futils::wrap::path_string libssl;
    futils::wrap::path_string libcrypto;
    std::string root_cert;
    load_root_cert(root_cert, libssl, libcrypto);
    fnet::tls::set_libcrypto(libcrypto.c_str());
    fnet::tls::set_libssl(libssl.c_str());
    auto result = fnet::connect("localhost", "8092", fnet::sockattr_udp(fnet::ip::Version::ipv6));
    if (!result) {
        cout << "failed to connect " << result.error().error();
        return 1;
    }
    auto conf = fnet::tls::configure_with_error();
    if (!conf) {
        cout << "failed to configure tls " << conf.error().error();
        return 1;
    }
    if (auto res = conf->set_cacert_file(root_cert.c_str()); !res) {
        cout << "failed to set cacert file " << res.error().error();
        return 1;
    }
    conf->set_alpn("\x02h3");
    auto q = use::smartptr::make_quic(std::move(*conf), [](std::shared_ptr<void> stream, fnet::quic::stream::StreamType ty) {});
    q->connect_start("localhost");
    auto [sock, addr] = std::move(result).value();
    constexpr auto timeFmt = "%Y-%M-%D %h:%m:%s ";
    auto time = [&] {
        return futils::timer::to_string<std::string>(timeFmt, futils::timer::utc_clock.now()) + " ";
    };
    cout << time() << "connected to " << addr.addr.to_string<std::string>() << "\n";
    auto local = sock.get_local_addr();
    if (!local) {
        cout << "failed to get local address " << local.error().error();
        return 1;
    }

    cout << time() << "local address " << local->to_string<std::string>() << "\n";

    while (true) {
        auto [data, _, idle] = q->create_udp_payload();
        if (!idle) {
            cout << time() << q->get_conn_error().error();
            break;
        }
        if (data.size()) {
            sock.writeto(addr.addr, data);
        }
        futils::byte buf[2000];
        auto rdata = sock.readfrom(buf);
        if (rdata) {
            if (rdata->first.size()) {
                q->parse_udp_payload(rdata->first);
            }
        }
    }
}
