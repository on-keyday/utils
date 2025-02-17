/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/json/json_export.h"
#include "../../include/json/literals.h"
#include "../../include/wrap/light/lite.h"
#include "../../include/json/convert_json.h"
#include "../../include/wrap/cout.h"

struct Test {
    int param1;
    futils::wrap::string param2;

    bool to_json(futils::json::JSON& js) const {
        JSON_PARAM_BEGIN(*this, js)
        TO_JSON_PARAM(param1, "param1")
        TO_JSON_PARAM(param2, "param2")
        JSON_PARAM_END()
    }
    bool from_json(const futils::json::JSON& v) {
        JSON_PARAM_BEGIN(*this, v)
        FROM_JSON_PARAM(param1, "param1")
        FROM_JSON_PARAM(param2, "param2")
        JSON_PARAM_END()
    }
};
using namespace futils::json;

bool to_json(const Test& t, JSON& js) {
    JSON_PARAM_BEGIN(t, js)
    TO_JSON_PARAM(param1, "param1")
    TO_JSON_PARAM(param2, "param2")
    JSON_PARAM_END()
}

void test_to_json() {
    JSON json, tos;
    Test test;
    test.param1 = 20;
    test.param2 = "hello";
    auto result1 = convert_to_json(test, json);
    assert(result1);
    json["param1"] = 40;
    json["param2"] = "call";
    auto result2 = convert_from_json(json, test);
    assert(result2);
    futils::wrap::cout_wrap() << to_string<futils::wrap::string>(json) << "\n";
    futils::wrap::map<futils::wrap::string, futils::wrap::string> val;
    auto result3 = convert_from_json(json, val, FromFlag::force_element);
    assert(result3);
    std::nullptr_t v;
    v = nullptr;
    convert_to_json(v, json);
    futils::wrap::cout_wrap() << to_string<futils::wrap::string>(json);
}

int main() {
    test_to_json();
}
