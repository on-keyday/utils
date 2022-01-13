/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/json/json_export.h"
#include "../../include/json/literals.h"
#include "../../include/wrap/lite/lite.h"
#include "../../include/json/to_json.h"
#include "../../include/wrap/cout.h"

struct Test {
    int param1;
    utils::wrap::string param2;
};
using namespace utils::json;

bool to_json(const Test& t, JSON& js) {
    auto e = js.at("param1");
    return true;
}

void test_to_json() {
    JSON json;
    json["param1"] = 20;
    json["param2"] = "hello";
    Test test;
    convert_to_json(test, json);
}

int main() {
    test_to_json();
}