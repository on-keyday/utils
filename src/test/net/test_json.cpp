/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/net/util/json/jsonbase.h"
#include "../../include/wrap/lite/lite.h"

void test_json() {
    namespace utw = utils::wrap;
    utils::net::json::JSONBase<utw::string, utw::vector, utw::map> json;
}

int main() {
    test_json();
}