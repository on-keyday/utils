/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <deprecated/net/async/dns.h>
#include <chrono>
#include <wrap/cout.h>

void test_asyncdns() {
    using namespace utils;
    auto result = net::query("httpbin.org", "http");
    auto begin = std::chrono::system_clock::now();
    auto address = result.get();
    auto end = std::chrono::system_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    utils::wrap::cout_wrap() << time.count() << "ms";
}

int main() {
    test_asyncdns();
}
