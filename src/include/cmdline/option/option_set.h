/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// option_set - option set
#pragma once

#include "../../wrap/lite/map.h"
#include "../../wrap/lite/string.h"
#include "../../wrap/lite/smart_ptr.h"
#include "../../wrap/lite/vector.h"
#include "optparser.h"

namespace utils {
    namespace cmdline {
        namespace option {
            struct Result;
            struct Option {
                wrap::string mainname;
                OptParser parser;
            };

            struct Description {
                wrap::map<wrap::string, wrap::shared_ptr<Option>> desc;
            };

            struct Result {
                wrap::string as_name;
                wrap::shared_ptr<Option> desc;
                Value value;
                size_t set_count = 0;
                bool reserved = false;
            };

            struct Results {
                wrap::map<wrap::string, Result> reserved;
                wrap::vector<Result> result;
                wrap::string erropt;
                int index = 0;
            };

            struct Context {
                Description desc;
                Results results;
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
