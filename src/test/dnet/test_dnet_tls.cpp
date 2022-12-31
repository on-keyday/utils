/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/socket.h>
#include <dnet/tls.h>
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
    dnet::set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    dnet::set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
#endif
    dnet::TLS tls = dnet::create_tls();
    assert(tls);
    tls.set_cacert_file("./test/net/cacert.pem");
    tls.set_alpn("\x08http/1.1", 9);
    tls.set_hostname("www.google.com");
    auto res = tls.make_ssl();
    assert(!res.is_error());
    char buf[1024 * 3];
    auto provider_loop = [&] {
        if (tls.receive_tls_data(buf, sizeof(buf)).is_noerr()) {
            sock.write(view::rvec(buf, tls.readsize()));
        }
        view::wvec v;
        dnet::error::Error err;
        while (true) {
            std::tie(v, err) = sock.read(buf);
            if (!err) {
                break;
            }
            assert(dnet::isSysBlock(err));
            std::this_thread::yield();
        }
        tls.provide_tls_data(buf, v.size());
    };
    while (auto err = tls.connect()) {
        assert(dnet::isTLSBlock(err));
        provider_loop();
    }
    constexpr auto data = "GET / HTTP/1.1\r\nHost: www.google.com\r\n\r\n";
    tls.write(data, strlen(data));
    provider_loop();
    while (auto err = tls.read(buf, sizeof(buf))) {
        assert(dnet::isTLSBlock(err));
        provider_loop();
    }
}
