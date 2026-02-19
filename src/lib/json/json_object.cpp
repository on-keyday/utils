/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#define FUTILS_JSON_NO_EXTERN_TEMPLATE
#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/json/json_export.h"
namespace futils::json {
}  // namespace futils::json

template <class String, template <class...> class Vec, template <class...> class Object>
void instantiate_json(futils::json::JSONBase<String, Vec, Object>& obj) {
    using self_t = futils::json::JSONBase<String, Vec, Object>;
    obj = nullptr;
    obj = true;
    obj = 0;
    obj = std::int64_t(0);
    obj = 0.9;
    obj = std::uint64_t(0);
    obj = String();
    obj = Vec<self_t>{};
    obj = Object<String, self_t>{};
    bool b;
    obj.as_bool(b);
    obj.at(String{});
    obj.at(0);
    obj[String{}];
    obj[0];
    obj.pop_back();
    obj.erase(0);
    obj.size();
    obj.is_undef();
    obj.is_bool();
    obj.is_number();
    obj.is_float();
    obj.is_string();
    obj.is_object();
    obj.is_array();
    obj.obegin();
    obj.oend();
    obj.abegin();
    obj.aend();
    obj.force_as_bool(b);
}

void instantiate() {
    using namespace futils::json;
    using namespace futils::json::literals;
    JSON json;
    instantiate_json(json);
    OrderedJSON ojson;
    instantiate_json(ojson);
    "{}"_json;
    "{}"_ojson;
}
