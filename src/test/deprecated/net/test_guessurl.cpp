/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <net_util/uri.h>

void test_guessurl() {
    using namespace utils::net::internal;
    auto sug = guess_uri_structure("google.com");
    assert(sug.suggest.size() == 1 &&
           sug.suggest[0] == URIParts::host);
    sug = guess_uri_structure("https://google.com:443/?q=0#top");
    sug = guess_uri_structure("file:///D:/CMakeLists.txt");
    sug = guess_uri_structure("mailto://root:password@gmail.com:80");
    sug = guess_uri_structure("tel://020-9392-4394");
    sug = guess_uri_structure("localhost:8090");
    sug = guess_uri_structure("?q=1");
    sug = guess_uri_structure("mailto:root@toguess.com");
}

int main() {
    test_guessurl();
}
