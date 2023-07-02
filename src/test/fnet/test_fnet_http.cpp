/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <map>
#include <string>
#include <testutil/timer.h>
#include <wrap/cout.h>
#include <vector>
#include <fnet/util/uri.h>
#include <strutil/equal.h>
using namespace utils;
/*
template <size_t len>
void test_host(uri::URIFixed<char, len> uri) {
    auto& cout = wrap::cout_wrap();
    fnet::HTTPRoundTrip http;
    test::Timer timer;
    std::vector<std::pair<const char*, std::chrono::milliseconds>> deltas;
    auto write_delta = [&](auto desc) {
        deltas.push_back({desc, timer.delta()});
    };
    auto flush_delta = [&] {
        cout << uri.hostname << "\n";
        cout << http.conn.addr.string() << "\n";
        for (auto& delta : deltas) {
            cout << delta.first << ": " << delta.second.count() << "\n";
        }
    };
    const char* scheme = nullptr;
    if (strutil::equal(uri.scheme, "http:")) {
        scheme = "http";
    }
    else if (strutil::equal(uri.scheme, "https:")) {
        scheme = "https";
    }
    assert(scheme);
    const char* port = nullptr;
    if (uri.port.size()) {
        port = uri.port.c_str();
    }
    else {
        if (strutil::equal(scheme, "http")) {
            port = "80";
        }
        else {
            port = "443";
        }
    }
    http.conn.start_resolve(uri.hostname.c_str(), port);
    while (!http.conn.wait_for_resolve(10)) {
        assert(!http.failed());
    }
    write_delta("resolve");
    http.conn.start_connect();
    while (!http.conn.wait_for_connect(1, 1)) {
        assert(!http.failed());
    }
    write_delta("connect");
    http.conn.set_cert("src/test/net/cacert.pem");
    http.start_tls(uri.hostname.c_str());
    write_delta("tls_start");
    while (!http.conn.wait_for_tls()) {
        assert(!http.failed());
    }
    write_delta("tls");
    std::map<std::string, std::string> h;
    h["Host"] = uri.hostname.c_str();
    http.request("GET", "/", h);
    http.start_sending();
    while (!http.wait_for_sending_done()) {
        assert(!http.failed());
    }
    write_delta("request");
    while (!http.wait_for_receiving()) {
        if (http.failed()) {
            assert(!http.failed());
        }
    }
    write_delta("response");
    flush_delta();
}

void test_fnet_http() {
#ifdef _WIN32
    fnet::set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    fnet::set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
#endif
    auto res = fnet::load_ssl();
    assert(res);
    test_host(uri::fixed("https://docs.microsoft.com/"));
    test_host(uri::fixed("https://www.google.com/"));
    test_host(uri::fixed("https://kantei.go.jp/"));
    test_host(uri::fixed("https://syosetu.com/"));
    test_host(uri::fixed("https://stackoverflow.com/"));
}
*/
void test_uri_parse() {
    constexpr auto test_parse1 = utils::uri::fixed("file:///C:/user/pc/workspace/yes.json");
    constexpr auto test_ok1 =
        utils::strutil::equal(test_parse1.scheme, "file:") &&
        utils::strutil::equal(test_parse1.authority, "") &&
        utils::strutil::equal(test_parse1.path, "/C:/user/pc/workspace/yes.json");
    constexpr auto test_parse2 = utils::uri::fixed(u8"https://Go言語.com:443/man?q=fmt.Printf#");
    constexpr auto test_ok2 =
        utils::strutil::equal(test_parse2.scheme, "https:") &&
        utils::strutil::equal(test_parse2.authority, u8"Go言語.com:443") &&
        utils::strutil::equal(test_parse2.hostname, u8"Go言語.com") &&
        utils::strutil::equal(test_parse2.port, ":443") &&
        utils::strutil::equal(test_parse2.path, "/man") &&
        utils::strutil::equal(test_parse2.query, "?q=fmt.Printf") &&
        utils::strutil::equal(test_parse2.fragment, "#");
    constexpr auto test_parse3 = utils::uri::fixed(R"a(javascript:alert("function"))a");
    constexpr auto test_ok3 =
        utils::strutil::equal(test_parse3.scheme, "javascript:") &&
        utils::strutil::equal(test_parse3.path, R"(alert("function"))");
    static_assert(test_ok1 && test_ok2 && test_ok3);
}

int main() {
    // test_fnet_http();
    test_uri_parse();
}
