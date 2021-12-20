/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/tcp/tcp.h"
#include "../../include/net/ssl/ssl.h"

void test_netcore() {
    utils::net::query_dns("google.com", "http");
}

int main() {
    test_netcore();
}