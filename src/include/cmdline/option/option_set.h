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
#include "../../utf/convert.h"

namespace utils {
    namespace cmdline {
        namespace option {
            struct Option {
                wrap::string mainname;
                wrap::vector<wrap::string> aliases;
                OptParser parser;
                wrap::string help;
                wrap::string argdesc;
            };

            struct Description {
                wrap::map<wrap::string, wrap::shared_ptr<Option>> desc;
                wrap::vector<wrap::shared_ptr<Option>> list;

                wrap::shared_ptr<Option> set(auto& option, OptParser parser, auto&& help, auto&& argdesc) {
                    auto spltview = helper::make_ref_splitview(option, ",");
                    auto mainname = utf::convert<wrap::string>(spltview[0]);
                    if (desc.find(mainname) != desc.end()) {
                        return nullptr;
                    }
                    wrap::vector<wrap::string> cvtvec;
                    auto sz = spltview.size();
                    for (size_t i = 1; i < sz; i++) {
                        cvtvec.push_back(utf::convert<wrap::string>(spltview[i]));
                        if (desc.find(cvtvec.back()) != desc.end()) {
                            return nullptr;
                        }
                    }
                    auto opt = wrap::make_shared<Option>();
                    opt->mainname = std::move(mainname);
                    opt->aliases = std::move(cvtvec);
                    opt->parser = std::move(parser);
                    opt->help = utf::convert<wrap::string>(help);
                    opt->argdesc = utf::convert<wrap::string>(argdesc);
                    desc[opt->mainname] = opt;
                    for (size_t i = 0; i < opt->aliases.size(); i++) {
                        desc[opt->aliases[i]] = opt;
                    }
                    list.push_back(opt);
                    return opt;
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

        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
