/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/json/json_export.h"
#include "../../include/json/literals.h"
#include "../../include/wrap/light/lite.h"
#include "../../include/json/parse.h"
#include "../../include/json/to_string.h"
#include "../../include/wrap/cout.h"

void test_json() {
    namespace utw = futils::wrap;
    futils::json::JSON json;
    auto e = futils::json::parse(
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
    using namespace futils;
    json::Stringer<> s;
    s.set_indent("    ");
    json::to_string(json, s);
    auto& cout = futils::wrap::cout_wrap();
    cout << s.out() << "\n";
    using namespace json::literals;
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
    js["url"] = "https://developer.mozilla.org/ja/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify";
    cout << json::to_string<futils::wrap::string>(js, json::FmtFlag::no_line | json::FmtFlag::unescape_slash)
         << "\n";
}

int main() {
    test_json();
}
