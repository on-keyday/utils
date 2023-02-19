/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/socket.h>
#include <fnet/tls/tls.h>
#include <fnet/addrinfo.h>
#include <cstring>
#include <cassert>
#include <thread>

using namespace utils;

int main() {
    fnet::SockAddr addr{};
    constexpr auto host = "www.google.com";
    auto resolve = fnet::resolve_address(host, "https", fnet::sockattr_tcp());
    assert(resolve.second.is_noerr());
    auto [list, err] = resolve.first.wait();
    assert(!err);
    fnet::Socket sock;
    while (list.next()) {
        addr = list.sockaddr();
        auto tmp = fnet::make_socket(addr.attr);
        auto err = tmp.connect(addr.addr);
        if (!err) {
            sock = std::move(tmp);
            break;
        }
        if (!fnet::isSysBlock(err) || tmp.wait_writable(10, 0).is_error()) {
            continue;
        }
        sock = std::move(tmp);
        break;
    }
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
        auto [read, err] = tls.receive_tls_data(buf);
        if (!err) {
            sock.write(read);
        }
        view::wvec v;
        while (true) {
            std::tie(v, err) = sock.read(buf);
            if (!err) {
                break;
            }
            assert(fnet::isSysBlock(err));
            std::this_thread::yield();
        }
        tls.provide_tls_data(v);
    };
    while (auto err = tls.connect()) {
        assert(fnet::tls::isTLSBlock(err));
        provider_loop();
    }
    constexpr auto data = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
    tls.write(data);
    provider_loop();
    while (true) {
        auto [red, err] = tls.read(buf);
        if (!err) {
            break;
        }
        assert(fnet::tls::isTLSBlock(err));
        provider_loop();
    }
}
