/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - define json object
#pragma once

#include "jsonbase.h"
#include "../wrap/lite/string.h"
#include "../wrap/lite/map.h"
#include "../wrap/lite/vector.h"
#include <utility>
#include <algorithm>
#include "../helper/equal.h"
#include "parse.h"
#include "to_string.h"
#include "ordered_map.h"

namespace utils {

    namespace json {
        using JSON = JSONBase<wrap::string, wrap::vector, wrap::map>;

        using OrderedJSON = JSONBase<wrap::string, wrap::vector, ordered_map>;

    }  // namespace json
}  // namespace utils
