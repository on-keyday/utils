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
            };

            struct Results {
                wrap::vector<Result> result;
            };

            struct Context {
                Description desc;
                Results results;
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
