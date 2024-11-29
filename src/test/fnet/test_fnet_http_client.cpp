/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/http/client.h>
#include <fnet/event/io.h>
#include <env/env_sys.h>

using URI = futils::fnet::http::URI;
int test_case_1(futils::fnet::http::Client& client) {
    auto r = client.request(
        "http://www.google.com",
        futils::fnet::http::UserCallback(
            nullptr, nullptr,
            [](void* c, const URI& uri, futils::fnet::http::RequestWriter& w) {
                w.write_request("GET", uri.path, [&](auto&& set) {
                    set("Host", uri.host);
                });
            },
            [](void* c, const URI& uri, futils::fnet::http::ResponseReader& r) {
                if (r.state() == futils::fnet::http1::HTTPState::body) {
                    r.read_body([](auto&&...) {});
                }
                else {
                    futils::fnet::http1::header::StatusCode code;
                    r.read_response(code, [](auto&&...) {});
                }
            }));
    if (r) {
    }
    while (!r->done()) {
        futils::fnet::wait_io_event(1);
    }
    r->wait().value();
    return 0;
}

int main() {
    futils::fnet::http::Client client;
    test_case_1(client);
    auto libssl = futils::env::sys::env_getter().get_or<futils::wrap::path_string>("FNET_LIBSSL", "libssl.so");
    auto libcrypto = futils::env::sys::env_getter().get_or<futils::wrap::path_string>("FNET_LIBCRYPTO", "libcrypto.so");
    futils::fnet::tls::set_libcrypto(libcrypto.c_str());
    futils::fnet::tls::set_libssl(libssl.c_str());
    auto conf = futils::fnet::tls::configure_with_error();
    if (!conf) {
        conf.value();
        return 1;
    }
    auto cacert = futils::env::sys::env_getter().get_or<std::string>("FNET_CA_CERT_FILE", "ca-cert.pem");
    conf->set_cacert_file(cacert.c_str());

    // client.request("https://www.google.com");
    // client.request("https://www.google.com/teapot")
}
