/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parsers - standard parsers
#pragma once

#include "option_set.h"
#include "../../helper/equal.h"

namespace utils {
    namespace cmdline {
        namespace option {
            struct BoolParser {
                bool is_parsable(int index, int argc, char** argv, const wrap::string& optname) {
                    if (helper::equal(argv[index] + 1, optname)) {
                    }
                }
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils