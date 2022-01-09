/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/net/util/json/json.h"
#include "../../include/wrap/lite/lite.h"
#include "../../include/net/util/json/parse.h"

void test_json() {
    namespace utw = utils::wrap;
    utils::net::json::JSON json;
    auto e = utils::net::json::parse(
        R"({
            "json": [
                "is",
                92,
                null,
                true,
                false,
                -9483,
                0.5,
                "object"
            ]
        })",
        json);
}

int main() {
    test_json();
}