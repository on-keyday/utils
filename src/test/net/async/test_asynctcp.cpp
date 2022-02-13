/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <net/async/tcp.h>
#include <wrap/cout.h>
#include <chrono>

void test_asynctcp() {
    using namespace utils;
    using namespace std::chrono;
    auto co = net::async_open("httpbin.org", "http");
    auto begin = system_clock::now();
    co.wait();
    auto end = system_clock::now();
    auto time = duration_cast<milliseconds>(end - begin);
    utils::wrap::cout_wrap() << time;
}

int main() {
    test_asynctcp();
}