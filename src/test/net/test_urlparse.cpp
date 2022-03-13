/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net_util/uri.h"

void test_urlparse() {
    utils::net::URI url;
    utils::net::rough_uri_parse("google.com", url);
    assert(url.host == "google.com" && "expect google.com but not");
    url = {};
    utils::net::rough_uri_parse("https://on-keyday:pass@gmail.com:443/?a=b#tag", url);
    assert(url.scheme == "https");
    assert(url.has_dobule_slash == true);
    assert(url.user == "on-keyday");
    assert(url.password == "pass");
    assert(url.host == "gmail.com");
    assert(url.port == ":443");
    assert(url.path == "/");
    assert(url.query == "?a=b");
    assert(url.tag == "#tag");
    assert(url.other == "");
    url = {};
    utils::net::rough_uri_parse("file:///D:/Minitools/Utils/test.txt", url);
    assert(url.scheme == "file");
    assert(url.has_dobule_slash == true);
    assert(url.host == "");
    assert(url.path == "/D:/Minitools/Utils/test.txt");
    url = {};
    utils::net::rough_uri_parse(R"(javascript:alert("hogehoge"))", url);
    assert(url.scheme == "javascript");
    assert(url.has_dobule_slash == false);
    assert(url.other == R"(alert("hogehoge"))");
    url = {};
    utils::net::rough_uri_parse("parsed/direct/bool.h", url);
    assert(url.path == "parsed/direct/bool.h");
}

int main() {
    test_urlparse();
}
