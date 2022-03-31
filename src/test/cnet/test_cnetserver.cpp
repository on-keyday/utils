/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/tcp.h>
using namespace utils;

void test_cnet_server() {
    auto server = cnet::tcp::create_server();
    cnet::tcp::set_port(server, "8080");
}

int main() {
}