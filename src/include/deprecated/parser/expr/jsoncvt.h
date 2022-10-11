/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// jsoncvt - json convert
#pragma once
#include <json/convert_json.h>
#include "expression.h"

namespace utils {
    namespace parser {
        namespace expr {
            bool to_json(const Expr& ptr, auto& json) {
                using js = std::remove_cvref_t<decltype(json)>;
                using str = typename js::string_t;
                str name;
                ptr.stringify(name);
                json["type"] = ptr.type();
                json["value"] = std::move(name);
                size_t i = 0;
                js* val = nullptr;
                while (auto v = ptr.index(i)) {
                    if (i == 0) {
                        val = &json["child"];
                    }
                    val->push_back(json::convert_to_json<js>(*v));
                    i++;
                }
                return true;
            }
        }  // namespace expr
    }      // namespace parser
}  // namespace utils
