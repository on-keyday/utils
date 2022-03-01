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
#include "../../wrap/pair_iter.h"
#include "optparser.h"
#include "../../utf/convert.h"

#include <algorithm>

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

            bool make_cvtvec(auto& option, auto& mainname, auto& desc, auto& cvtvec) {
                auto spltview = helper::make_ref_splitview(option, ",");
                mainname = utf::convert<wrap::string>(spltview[0]);
                if (desc.find(mainname) != desc.end()) {
                    return false;
                }
                auto sz = spltview.size();
                for (size_t i = 1; i < sz; i++) {
                    cvtvec.push_back(utf::convert<wrap::string>(spltview[i]));
                    if (desc.find(cvtvec.back()) != desc.end()) {
                        return false;
                    }
                }
                return true;
            }

            struct Description {
                wrap::map<wrap::string, wrap::shared_ptr<Option>> desc;
                wrap::vector<wrap::shared_ptr<Option>> list;

                wrap::shared_ptr<Option> set(auto& option, OptParser parser, auto&& help, auto&& argdesc) {
                    wrap::string mainname;
                    wrap::vector<wrap::string> cvtvec;
                    if (!make_cvtvec(option, mainname, desc, cvtvec)) {
                        return nullptr;
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

               private:
                auto sorter() {
                    return [](auto& a, auto& b) {
                        return a.desc->mainname < b.desc->mainname;
                    };
                }
                void sort_result() {
                    if (!std::is_sorted(result.begin(), result.end(), sorter())) {
                        std::sort(result.begin(), result.end(), sorter());
                    }
                }

               public:
                auto find(auto&& name) {
                    sort_result();
                    auto check = [&](auto&& v) {
                        return helper::equal(v.desc->mainname, name);
                    };
                    auto begiter = std::find_if(result.begin(), result.end(), check);
                    if (begiter == result.end()) {
                        return wrap::iter(std::pair{begiter, begiter});
                    }
                    auto enditer = begiter;
                    enditer++;
                    for (; enditer != result.end(); enditer++) {
                        if (!check(*enditer)) {
                            break;
                        }
                    }
                    return wrap::iter(std::pair{begiter, enditer});
                }
            };

        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
