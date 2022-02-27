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
#include "utf/convert.h"

namespace utils {
    namespace cmdline {
        namespace option {
            struct Result;
            struct Option {
                wrap::string mainname;
                wrap::vector<wrap::string> aliases;
                OptParser parser;
                wrap::string help;
                wrap::string argdesc;
            };

            struct Description {
                wrap::map<wrap::string, wrap::shared_ptr<Option>> desc;

                bool set(wrap::string& name, OptParser parser) {
                    auto spltview = helper::make_ref_splitview(name, ",");
                    auto mainname = utf::convert<wrap::string>(spltview[0]);
                    if (desc.find(mainname) != desc.end()) {
                        return false;
                    }
                    wrap::vector<wrap::string> cvtvec;
                    auto sz = spltview.size();
                    for (size_t i = 1; i < sz; i++) {
                        cvtvec.push_back(utf::convert<wrap::string>(spltview[i]));
                        if (desc.find(cvtvec.back()) != desc.end()) {
                            return false;
                        }
                    }
                    auto opt = wrap::make_shared<Option>();
                    opt->mainname = std::move(mainname);
                    opt->aliases = std::move(cvtvec);
                    opt->parser = std::move(parser);
                }
            };

            struct Result {
                wrap::string as_name;
                wrap::shared_ptr<Option> desc;
                SafeVal<Value> value;
                size_t set_count = 0;
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
