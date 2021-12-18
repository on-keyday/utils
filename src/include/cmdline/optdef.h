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
#include "../helper/appender.h"

namespace utils {
    namespace cmdline {
        enum class OptFlag {
            none = 0,
            required = 0x1,        // option must set
            must_assign = 0x2,     // value must set with `=`
            no_option_like = 0x4,  // disallow string begin with `-`
            once_in_cmd = 0x8,     // allow only once to set
            need_value = 0x10,     // need value
        };

        DEFINE_ENUM_FLAGOP(OptFlag)

        template <class String, template <class...> class Vec>
        struct Option {
            String mainname;
            OptValue<> defvalue;
            String help;
            String valdesc;
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
            template <class Sep = const char*, class CmdName = Sep>
            String help(CmdName cmdname = "command", Sep cmdusage = "[option]", Sep usage = "Usage:", Sep option = "Option:",
                        Sep sepflag = ", ", Sep sephelp = ": ", size_t indent = 0, size_t tablen = 4) {
                String result;
                helper::append(result, helper::CharView(' ', indent));
                helper::append(result, usage);
                helper::append(result, "\n");
                helper::append(result, helper::CharView(' ', indent + tablen));
                helper::append(result, cmdname);
                helper::append(result, " ");
                helper::append(result, cmdusage);
                helper::append(result, "\n");
                helper::append(result, helper::CharView(' ', indent));
                helper::append(result, option);
                helper::append(result, "\n");
                helper::append(result, option_help(sepflag, sephelp, indent, tablen));
                return result;
            }

            template <class Sep = const char*>
            String option_help(Sep sepflag = ", ", Sep sephelp = ": ", size_t indent = 0, size_t tablen = 4) {
                auto write_a_option = [&](auto& result, auto& v, auto& opt) {
                    if (v.size() == 1) {
                        helper::append(result, "-");
                    }
                    else {
                        helper::append(result, "--");
                    }
                    helper::append(result, v);
                    bool need_val = any(opt->flag & OptFlag::need_value);
                    bool must_as = any(opt->flag & OptFlag::must_assign);
                    if (auto v = opt->defvalue.template value<bool>()) {
                        if (need_val) {
                            if (must_as) {
                                helper::append(result, "=");
                            }
                            else {
                                helper::append(result, " ");
                            }
                            helper::append(result, *v ? "true" : "false");
                        }
                    }
                    else {
                        if (must_as) {
                            helper::append(result, "=");
                        }
                        else {
                            helper::append(result, " ");
                        }
                        if (opt->valdesc.size()) {
                            helper::append(result, opt->valdesc);
                        }
                        else {
                            helper::append(result, "VALUE");
                        }
                    }
                };
                auto set_desc = [&](auto& opt, auto& result) {
                    size_t count = 0;
                    for (size_t i = 0; i < opt->alias.size(); i++) {
                        auto& v = opt->alias[i];
                        if (v.size() != 1) {
                            continue;
                        }
                        if (count != 0) {
                            helper::append(result, sepflag);
                        }
                        write_a_option(result, v, opt);
                        count++;
                    }
                    for (size_t i = 0; i < opt->alias.size(); i++) {
                        auto& v = opt->alias[i];
                        if (v.size() == 1) {
                            continue;
                        }
                        if (count != 0) {
                            helper::append(result, sepflag);
                        }
                        write_a_option(result, v, opt);
                        count++;
                    }
                };
                String result;
                size_t maxlen = 0;
                for (option_t& opt : vec) {
                    helper::CountPushBacker counter;
                    set_desc(opt, counter);
                    if (counter.count > maxlen) {
                        maxlen = counter.count;
                    }
                }
                for (option_t& opt : vec) {
                    helper::CountPushBacker<String&> pb{result};
                    helper::append(result, helper::CharView(' ', indent + tablen));
                    set_desc(opt, pb);
                    auto remain = maxlen + 1 - pb.count;
                    helper::append(result, helper::CharView(' ', remain));
                    helper::append(result, sephelp);
                    helper::append(result, opt->help);
                    helper::append(result, "\n");
                }
                return result;
            }

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
            OptValue<> value_;

            size_t count() const {
                if (auto vec = value.template value<Vec<OptValue<>>>()) {
                    return vec->size();
                }
                return 1;
            }

            template <class T>
            T* value() {
                if (auto v = value.template value<T>()) {
                    return v;
                }
                return nullptr;
            }
        };

        enum class ParseFlag {
            // the word `longname` means option name which length is not 1

            none,
            two_prefix_longname = 0x1,      // option begin with `--` is long name
            allow_assign = 0x2,             // allow `=` operator
            adjacent_value = 0x4,           // `-oValue` become `o` option with value `Value` (default `-oValue` become o,V,a,l,u,e option)
            ignore_after_two_prefix = 0x8,  // after `--` is not option
            one_prefix_longname = 0x10,     // option begin with `-` is long name
            ignore_not_found = 0x20,        // ignore if option is not found
            parse_all = 0x40,               // parse all arg
            failure_opt_as_arg = 0x80,      // failed to parse arg is argument

            optget_mode = two_prefix_longname | ignore_after_two_prefix | parse_all | allow_assign,

            // one_prefix_longname and adjacent_value are not settable at the same time
            // one_prefix_longname has priority
        };

        DEFINE_ENUM_FLAGOP(ParseFlag)

        enum class ParseError {
            none,
            suspend_parse,
            not_one_opt,
            not_assigned,
            option_like_value,
            need_value,
            require_more_argument,
            not_found,
            invalid_value,
            wrong_value,
            unexpected_type,
            wrong_assign,
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionSet;
        namespace internal {
            template <class String, class Char, template <class...> class Map, template <class...> class Vec>
            ParseError parse_one(int& index, int argc, Char** argv, wrap::shared_ptr<Option<String, Vec>>& opt,
                                 OptionSet<String, Vec, Map>& result,
                                 ParseFlag flag, String* assign);
        }

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionSet {
           private:
            Map<String, OptionResult<String, Vec>> result;

            template <class Str, class Char, template <class...> class Maps, template <class...> class Vecs>
            friend ParseError internal::parse_one(int& index, int argc, Char** argv, wrap::shared_ptr<Option<Str, Vecs>>& opt,
                                                  OptionSet<Str, Vecs, Maps>& result,
                                                  ParseFlag flag, Str* assign);

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

           public:
        };
    }  // namespace cmdline

}  // namespace utils
