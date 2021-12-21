/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net/util/url.h"

void test_urlparse() {
    utils::net::URL url;
    utils::net::rough_url_parse("google.com", url);
    assert(url.host == "google.com" && "expect google.com but not");
}

int main() {
    test_urlparse();
}