/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/socket.h>
#include <fnet/tls/tls.h>
#include <fnet/addrinfo.h>
#include <cstring>
#include <cassert>
#include <thread>
#include <fnet/connect.h>

using namespace futils;

int main() {
    fnet::SockAddr addr{};
    constexpr auto host = "www.google.com";
    auto sock = fnet::connect(host, "https", fnet::sockattr_tcp()).value().first;

#ifdef _WIN32
    fnet::tls::set_libcrypto(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/crypto.dll"));
    fnet::tls::set_libssl(fnet_lazy_dll_path("D:/quictls/boringssl/built/lib/ssl.dll"));
#endif
    auto config = fnet::tls::configure();
    config.set_cacert_file("./test/net/cacert.pem");
    fnet::tls::TLS tls = fnet::tls::create_tls(config);
    assert(tls);
    tls.set_alpn("\x08http/1.1");
    tls.set_hostname("www.google.com");
    char buf[1024 * 3];
    auto provider_loop = [&] {
        auto read = tls.receive_tls_data(buf);
        if (read) {
            sock.write(*read);
        }
        view::wvec v;
        while (true) {
            auto data = sock.read(buf);
            if (data) {
                v = *data;
                break;
            }
            assert(fnet::isSysBlock(data.error()));
            std::this_thread::yield();
        }
        tls.provide_tls_data(v);
    };
    auto err = tls.connect();
    while (!err) {
        assert(fnet::tls::isTLSBlock(err.error()));
        provider_loop();
        err = tls.connect();
    }
    constexpr auto data = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
    tls.write(data);
    provider_loop();
    while (true) {
        auto data = tls.read(buf);
        if (data) {
            break;
        }
        assert(fnet::tls::isTLSBlock(data.error()));
        provider_loop();
    }
}
