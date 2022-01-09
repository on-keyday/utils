/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/json/json_export.h"

template <class String, template <class...> class Vec, template <class...> class Object>
void instantiate_json(utils::json::JSONBase<String, Vec, Object>& obj) {
}

void instantiate() {
    using namespace utils::json;
    JSON json;
    instantiate_json(json);
    OrderedJSON ojson;
    instantiate_json(ojson);
}