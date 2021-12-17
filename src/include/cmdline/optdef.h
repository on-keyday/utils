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

        template <class String>
        struct Option {
            OptValue<> defvalue;
            String help;
            OptFlag flag = OptFlag::none;
        };

        template <template <class...> class Vec, class T>
        struct VecOption {
            Vec<T> defval;
            size_t minimum = 0;
        };

        template <template <class...> class Vec, class T>
        VecOption<Vec, T> multivalue(size_t maximum, size_t miniimum = ~0, Vec<T>&& def = Vec<T>{}) {
            VecOption<Vec, T> ret;
            ret.defval.resize(maximum);
            ret.minimum = miniimum;
            for (size_t i = 0; i < maximum; i++) {
                ret.defval[i] = std::move(def[i]);
            }
            return ret;
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionDesc {
            using option_t = wrap::shared_ptr<Option<String>>;
            Vec<option_t> vec;
            Map<String, option_t> desc;

            template <class Str, class Help = Str>
            OptionDesc& operator()(Str&& name, OptValue<> defvalue, Help&& help, OptFlag flag) {
                auto alias = helper::split<Str, Vec>(utf::convert<String>(name), ",");
                for (auto& n : alias) {
                    if (n == Str()) {
                        return *this;
                    }
                    if (desc.find(n) != desc.end()) {
                        return *this;
                    }
                }
                auto opt = wrap::make_shared<Option<String>>();
                opt->defvalue = std::move(defvalue);
                opt->flag = flag;
                opt->help = utf::convert<String>(help);
                vec.push_back(opt);
                for (auto& n : alias) {
                    desc.emplace(n, opt);
                }
                return *this;
            }

            bool find(auto& name, option_t& opt) {
                if (auto found = desc.find(name); found != desc.end()) {
                    opt = std::get<1>(*found);
                    return true;
                }
                return false;
            }
        };

        template <class String>
        struct OptionResult {
            using option_t = wrap::shared_ptr<Option<String>>;
            option_t base;
            OptValue<> value;
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionSet {
            Map<String, OptValue<>> result;

            void emplace(auto& name, OptValue<>*& opt) {
                opt = &result[name]
            }

            bool find(auto& name, OptValue<>*& opt) {
                if (auto found = result.find(name); found != result.end()) {
                    opt = &std::get<1>(*found);
                    return true;
                }
                return false;
            }
        };
    }  // namespace cmdline

}  // namespace utils
