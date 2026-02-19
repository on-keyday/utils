/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/json/json_export.h"
#include "../../include/wrap/light/lite.h"
#include "../../include/json/path.h"
#include "../../include/json/to_string.h"
#include "../../include/wrap/cout.h"

void test_jsonpath() {
    using namespace futils::json;
    JSON js;
    auto obj = path(js, R"(.object["object"][0])", true);
    assert(obj);
    *obj = "string";
    futils::wrap::cout_wrap() << to_string<futils::wrap::string>(js);
}

int main() {
    test_jsonpath();
}