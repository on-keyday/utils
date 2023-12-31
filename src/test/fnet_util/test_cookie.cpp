/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/util/cookie.h>
#include <fnet/util/uri.h>
#include <wrap/light/lite.h>

void test_cookie() {
    utils::wrap::vector<utils::http::cookie::Cookie<utils::wrap::string>> cookies;
    utils::wrap::map<utils::wrap::string, utils::wrap::string> header;
    header = {{"Set-Cookie", "id=a3fWa; Expires=Thu, 21 Oct 2021 07:28:00 GMT; Secure; HttpOnly"}};
    utils::uri::URI<std::string> uri;
    uri.path = "/";
    uri.hostname = "google.com";
    utils::http::cookie::parse_set_cookie(header, cookies, uri);
}

int main() {
    test_cookie();
}
