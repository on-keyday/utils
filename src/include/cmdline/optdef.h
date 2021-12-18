/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// optdef - option definition object
#pragma once

#include "optvalue.h"
#include "../utf/convert.h"
#include "../wrap/lite/smart_ptr.h"
#include "../helper/strutil.h"
#include "../helper/splits.h"
#include "../wrap/lite/enum.h"

namespace utils {
    namespace cmdline {
        enum class OptFlag {
            none = 0,
            required = 0x1,        // option must set
            must_assign = 0x2,     // value must set with `=`
            no_option_like = 0x4,  // `somevalue` type disallow string like `option`
            once_in_cmd = 0x8,     // allow only once to set
            need_value = 0x10,     // need value
        };

        DEFINE_ENUM_FLAGOP(OptFlag)

        template <class String, template <class...> class Vec>
        struct Option {
            String mainname;
            OptValue<> defvalue;
            String help;
            OptFlag flag = OptFlag::none;
            Vec<String> alias;
        };

        template <template <class...> class Vec, class T>
        struct VecOption {
            Vec<T> defval;
            size_t minimum = 0;
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionDesc {
            using option_t = wrap::shared_ptr<Option<String, Vec>>;

           private:
            Vec<option_t> vec;
            Map<String, size_t> desc;

           public:
            template <class Str, class Help = Str>
            OptionDesc& set(Str&& name, OptValue<> defvalue, Help&& help, OptFlag flag) {
                auto alias = helper::split<String, Vec>(utf::convert<String>(name), ",");
                if (alias.size() == 0) {
                    return *this;
                }
                for (auto& n : alias) {
                    if (helper::equal(n, "")) {
                        return *this;
                    }
                    if (desc.find(n) != desc.end()) {
                        return *this;
                    }
                }
                auto opt = wrap::make_shared<Option<String, Vec>>();
                opt->mainname = alias[0];
                opt->defvalue = std::move(defvalue);
                opt->flag = flag;
                opt->help = utf::convert<String>(help);
                vec.push_back(opt);
                auto idx = vec.size() - 1;
                for (auto& n : alias) {
                    desc.emplace(n, idx);
                }
                opt->alias = std::move(alias);
                return *this;
            }

            bool find(const auto& name, option_t& opt) {
                if (auto found = desc.find(name); found != desc.end()) {
                    auto idx = std::get<1>(*found);
                    assert(idx < vec.size());
                    opt = vec[idx];
                    return true;
                }
                return false;
            }
        };

        template <class String, template <class...> class Vec>
        struct OptionResult {
            using option_t = wrap::shared_ptr<Option<String, Vec>>;
            option_t base;
            OptValue<> value;

            size_t count() const {
                if (auto vec = value.template value<Vec<OptValue<>>>()) {
                    return vec->size();
                }
                return 1;
            }
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionSet {
           private:
            Map<String, OptionResult<String, Vec>> result;

            void emplace(wrap::shared_ptr<Option<String, Vec>> option, OptionResult<String, Vec>*& res) {
                res = &result[option->mainname];
                res->base = std::move(option);
            }

            bool find(auto& name, OptionResult<String, Vec>*& opt) {
                if (auto found = result.find(name); found != result.end()) {
                    opt = &std::get<1>(*found);
                    return true;
                }
                return false;
            }
        };
    }  // namespace cmdline

}  // namespace utils
