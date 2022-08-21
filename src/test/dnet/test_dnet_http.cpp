/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/http.h>
#include <map>
#include <string>
#include <testutil/timer.h>
#include <wrap/cout.h>
#include <vector>
using namespace utils;

void test_host(const char* host) {
    auto& cout = wrap::cout_wrap();
    dnet::HTTP http;
    test::Timer timer;
    std::vector<std::pair<const char*, std::chrono::milliseconds>> deltas;
    auto write_delta = [&](auto desc) {
        deltas.push_back({desc, timer.delta()});
    };
    auto flush_delta = [&] {
        cout << host << "\n";
        for (auto& delta : deltas) {
            cout << delta.first << ": " << delta.second.count() << "\n";
        }
    };

    http.start_resolve(host, "443");
    while (!http.wait_for_resolve(10)) {
        assert(!http.failed());
    }
    write_delta("resolve");
    http.start_connect();
    while (!http.wait_for_connect(1, 1)) {
        assert(!http.failed());
    }
    write_delta("connect");
    http.set_cert("src/test/net/cacert.pem");
    http.start_tls(host);
    write_delta("tls_start");
    while (!http.wait_for_tls()) {
        assert(!http.failed());
    }
    write_delta("tls");
    std::map<std::string, std::string> h;
    h["Host"] = host;
    auto res = http.set_header(h);
    assert(res);
    http.request("GET", "/", nullptr, 0);
    while (!http.wait_for_request_done()) {
        assert(!http.failed());
    }
    write_delta("request");
    while (!http.wait_for_response()) {
        if (http.failed()) {
            assert(!http.failed());
        }
    }
    write_delta("response");
    flush_delta();
}

void test_dnet_http() {
#ifdef _WIN32
    dnet::set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    dnet::set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
#endif
    auto res = dnet::load_ssl();
    assert(res);
    test_host("docs.microsoft.com");
    test_host("www.google.com");
    test_host("www.kantei.go.jp");
    test_host("syosetu.com");
    test_host("stackoverflow.com");
}

int main() {
    test_dnet_http();
}
