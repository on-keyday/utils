/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// option_set - option set
#pragma once

#include "../../wrap/light/map.h"
#include "../../wrap/light/string.h"
#include "../../wrap/light/smart_ptr.h"
#include "../../wrap/light/vector.h"
#include "../../wrap/pair_iter.h"
#include "optparser.h"
#include "../../unicode/utf/convert.h"
#include "../../view/slice.h"

#include <algorithm>

namespace futils {
    namespace cmdline {
        namespace option {

            struct Option {
                wrap::string mainname;
                wrap::vector<wrap::string> aliases;
                OptParser parser;
                wrap::string help;
                wrap::string argdesc;
                OptMode mode;
            };

            bool make_cvtvec(auto& option, auto& mainname, auto& desc, auto& cvtvec) {
                auto spltview = view::make_ref_splitview(option, ",");
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

                wrap::shared_ptr<Option> set(auto& option, OptParser parser, auto&& help, auto&& argdesc, OptMode mode) {
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
                    opt->mode = mode;
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
                Result* get_result(auto&& name, size_t index = 0) {
                    auto found = reserved.find(name);
                    if (found != reserved.end()) {
                        return &get<1>(*found);
                    }
                    auto it = find(name);
                    if (!it.empty()) {
                        auto d = it.begin();
                        for (size_t i = 0; i < index && d != it.end(); i++, d++)
                            ;
                        if (d != it.end()) {
                            return &*d;
                        }
                    }
                    return nullptr;
                }

                template <class T>
                T* value_ptr(auto&& name, size_t index = 0) {
                    auto found = reserved.find(name);
                    if (found != reserved.end()) {
                        Result& place = get<1>(*found);
                        auto ptr = place.value.get_ptr<T>();
                        if (ptr) {
                            return ptr;
                        }
                    }
                    auto it = find(name);
                    if (!it.empty()) {
                        auto d = it.begin();
                        for (size_t i = 0; i < index && d != it.end(); i++, d++)
                            ;
                        if (d != it.end()) {
                            Result& place = *d;
                            auto ptr = place.value.get_ptr<T>();
                            if (ptr) {
                                return ptr;
                            }
                        }
                    }
                    return nullptr;
                }

                template <class T>
                T value_or_not(auto&& name, T or_not = T{}, size_t index = 0) {
                    auto ptr = value_ptr<std::remove_cvref_t<T>>(name, index);
                    if (!ptr) {
                        return or_not;
                    }
                    return *ptr;
                }

                auto find(auto&& name) {
                    sort_result();
                    auto check = [&](auto&& v) {
                        return strutil::equal(v.desc->mainname, name);
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
}  // namespace futils
