/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/net/util/json/json.h"
#include "../../include/wrap/lite/lite.h"
#include "../../include/net/util/json/parse.h"
#include "../../include/net/util/json/to_string.h"
#include "../../include/wrap/cout.h"

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
                "object",
                {
                    "nested":"value"
                }
            ]
        })",
        json);
    json["handle"] = 0;
    utils::wrap::string v;
    auto w = utils::helper::make_indent_writer(v, "    ");
    using namespace utils::net;
    utils::net::json::to_string(json, w);
    auto& cout = utils::wrap::cout_wrap();
    cout << v << "\n";
    using namespace utils::net::json::literals;
    auto js = R"({
      "\they yp": {
          "escape\n": null
      }  
    })"_ojson;
    js = R"({
  "browsers": {
    "firefox": {
      "name": "Firefox",
      "pref_url": "about:config",
      "releases": {
        "1": {
          "release_date": "2004-11-09",
          "status": "retired",
          "engine": "Gecko",
          "engine_version": "1.7"
        }
      }
    }
  }
})"_ojson;
    cout << json::to_string<utils::wrap::string>(js, json::FmtFlag::no_line)
         << "\n";
}

int main() {
    test_json();
}