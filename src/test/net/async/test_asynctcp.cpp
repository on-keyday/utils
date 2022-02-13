/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <net/async/tcp.h>
#include <chrono>

void test_asynctcp() {
    using namespace utils;
    net::async_open();
}

int main() {
    test_asynctcp();
}