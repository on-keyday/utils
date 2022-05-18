/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/tcp.h>
#include <thread>
#include <wrap/cout.h>
using namespace utils;
auto& cout = wrap::cout_wrap();

void test_cnet_server() {
    auto server = cnet::tcp::create_server();
    cnet::tcp::set_port(server, "8080");
    cnet::tcp::set_connect_timeout(server, 500);
    cnet::tcp::set_reuse_address(server, true);
    auto cb = [](cnet::CNet* ctx) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        return true;
    };
    cnet::set_lambda(server, cb);
    auto suc = cnet::open(server);
    assert(suc);
    while (true) {
        auto conn = cnet::tcp::accept(server);
        if (!conn) {
            if (cnet::tcp::is_timeout(server)) {
                continue;
            }
            cout << "error:" << cnet::get_error(server);
            assert(conn);
        }
        auto text = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 30\r\nContent-Type: text/html\r\n\r\n<h1>Server is not working</h1>";
        size_t w = 0;
        cnet::write({}, conn, text, ::strlen(text), &w);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        cnet::delete_cnet(conn);
    }
}

int main() {
    test_cnet_server();
}
