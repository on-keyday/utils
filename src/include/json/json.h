/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - define json object
#pragma once

#include "jsonbase.h"
#include "../wrap/light/string.h"
#include "../wrap/light/map.h"
#include "../wrap/light/vector.h"
#include <utility>
#include <algorithm>
#include "parse.h"
#include "to_string.h"
#include "ordered_map.h"

namespace futils {

    namespace json {
        using JSON = JSONBase<wrap::string, wrap::vector, wrap::map>;

        using OrderedJSON = JSONBase<wrap::string, wrap::vector, ordered_map>;

    }  // namespace json
}  // namespace futils
