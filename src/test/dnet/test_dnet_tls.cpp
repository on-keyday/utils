/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/socket.h>
#include <dnet/tls/tls.h>
#include <dnet/addrinfo.h>
#include <cstring>
#include <cassert>
#include <thread>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#endif
using namespace utils;

int main() {
    dnet::SockAddr addr{};
    constexpr auto host = "www.google.com";
    auto resolve = dnet::resolve_address(host, "https", {.socket_type = SOCK_STREAM});
    auto list = resolve.wait();
    assert(!resolve.failed());
    dnet::Socket sock;
    while (list.next()) {
        addr = list.sockaddr();
        auto tmp = dnet::make_socket(addr.attr);
        auto err = tmp.connect(addr.addr);
        if (!err) {
            sock = std::move(tmp);
            break;
        }
        if (!dnet::isSysBlock(err) || tmp.wait_writable(10, 0).is_error()) {
            continue;
        }
        sock = std::move(tmp);
        break;
    }
#ifdef _WIN32
    dnet::tls::set_libcrypto(dnet_lazy_dll_path("D:/quictls/boringssl/built/lib/crypto.dll"));
    dnet::tls::set_libssl(dnet_lazy_dll_path("D:/quictls/boringssl/built/lib/ssl.dll"));
#endif
    auto config = dnet::tls::configure();
    config.set_cacert_file("./test/net/cacert.pem");
    dnet::tls::TLS tls = dnet::tls::create_tls(config);
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
            assert(dnet::isSysBlock(err));
            std::this_thread::yield();
        }
        tls.provide_tls_data(v);
    };
    while (auto err = tls.connect()) {
        assert(dnet::tls::isTLSBlock(err));
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
        assert(dnet::tls::isTLSBlock(err));
        provider_loop();
    }
}
