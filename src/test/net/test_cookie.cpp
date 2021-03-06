/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/net_util/cookie.h"
#include "../../include/net_util/uri.h"
#include "../../include/wrap/light/lite.h"

void test_cookie() {
    utils::wrap::vector<utils::net::cookie::Cookie<utils::wrap::string>> cookies;
    utils::wrap::map<utils::wrap::string, utils::wrap::string> header;
    header = {{"Set-Cookie", "id=a3fWa; Expires=Thu, 21 Oct 2021 07:28:00 GMT; Secure; HttpOnly"}};
    utils::net::URI uri;
    uri.path = "/";
    uri.host = "google.com";
    utils::net::cookie::parse_set_cookie(header, cookies, uri);
}

int main() {
    test_cookie();
}
