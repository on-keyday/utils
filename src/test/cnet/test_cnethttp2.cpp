/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/http2.h>
#include <cnet/tcp.h>
#include <cnet/ssl.h>

void test_cnet_http2() {
    using namespace utils::cnet;
    auto tcp = tcp::create_client();
    tcp::set_hostport(tcp, "google.com", "https");
    bool ok = open(tcp);
    assert(ok);
    auto tls = ssl::create_client();
    ssl::set_certificate_file(tls, "src/test/net/cacert.pem");
    ssl::set_alpn(tls, "\2h2");
    ssl::set_host(tls, "google.com");
    set_lowlevel_protocol(tls, tcp);
    ok = open(tls);
    assert(ok);
    auto alpn = ssl::get_alpn_selected(tls);
    assert(strncmp(alpn, "h2", 2) == 0);
    auto client = http2::create_client();
    http2::set_frame_callback(client, [](http2::Frames* fr) {
        return false;
    });
    set_lowlevel_protocol(client, tls);
    ok = open(client);
    assert(ok);
}

int main() {
    test_cnet_http2();
}
