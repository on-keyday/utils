/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - parse option
#pragma once

#include "optvalue.h"
#include "../helper/strutil.h"
#include "../utf/convert.h"
#include "../wrap/lite/enum.h"
#include <cassert>

namespace utils {
    namespace cmdline {
        enum class ParseFlag {
            none,
            two_prefix_longname = 0x1,      // option begin with `--` is long name
            allow_assign = 0x2,             // allow `=` operator
            adjacent_value = 0x4,           // `-oValue` become `o` option with value `Value` (default `-oValue` become o,V,a,l,u,e option)
            ignore_after_two_prefix = 0x8,  // after `--` is not option
            one_prefix_longname = 0x10,     // option begin with `-` is long name
            ignore_not_found = 0x20,        // ignore if option is not found
            parse_all = 0x40,               // parse all arg
            failure_opt_as_arg = 0x80,      // failed to parse arg is argument
        };

        DEFINE_ENUM_FLAGOP(ParseFlag)

        enum class ParseError {
            none,
            suspend_parse,
            not_one_opt,
            not_assigned,
            option_like_value,
            need_value,
            bool_not_true_or_false,
            int_not_number,
            require_more_argument,
            not_found,
        };

        namespace internal {
            template <template <class...> class Vec, class String, class Char, class Value, class F>
            ParseError parse_value(int& index, int argc, Char** argv,
                                   wrap::shared_ptr<Option<String>>& opt,
                                   ParseFlag flag, String* assign, Value* b,
                                   OptValue<>* target, F&& set_value) {
                String cmp;
                Value result;
                bool need_less = false;
                if (any(opt->flag & OptFlag::must_assign) && !assign) {
                    if (any(opt->flag & OptFlag::need_value)) {
                        return ParseError::need_value;
                    }
                    result = *b;
                }
                else {
                    if (assign) {
                        cmp = std::move(*assign);
                    }
                    else {
                        cmp = utf::convert<String>(argv[index + 1]);
                        need_less = true;
                    }
                    if (set_value(result, cmp)) {
                        if (need_less) {
                            index++;
                        }
                    }
                    else if (need_less) {
                        if (any(opt->flag & OptFlag::need_value)) {
                            return ParseError::need_value;
                        }
                        result = *b;
                    }
                    else {
                        return ParseError::bool_not_true_or_false;
                    }
                }
                if (auto vec = target->template value<Vec<OptValue<>>>()) {
                    vec->push_back(result);
                }
                else {
                    *target = std::move(result);
                }
                +return ParseError::none;
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_bool(int& index, int argc, Char** argv,
                                  wrap::shared_ptr<Option<String>>& opt,
                                  ParseFlag flag, String* assign, bool* b,
                                  OptValue<>* target) {
                return parse_value(index, argc, argv, opt, flag, assign, b, target, [](auto& result, auto& value) {
                    if (helper::equal("true") || helper::equal("false")) {
                        result = value[0] == 't';
                        return true;
                    }
                    return false;
                });
            }
        }  // namespace internal

        template <class String, class Char, template <class...> class Map, template <class...> class Vec>
        ParseError parse_one(int& index, int argc, Char** argv, wrap::shared_ptr<Option<String>>& opt,
                             auto& name,
                             OptionSet<String, Vec, Map>& result,
                             ParseFlag flag, String* assign) {
            Option<String>& option = *opt;
            OptValue<>& def = option.defvalue;
            OptValue<>* target = nullptr;
            if (result.find(name, target)) {
                if (any(option.flag & OptFlag::once_in_cmd)) {
                    return ParseError::not_one_opt;
                }
                if (target->type() != type<Vec<OptValue<>>>()) {
                    auto tmp = std::move(*target);
                    *target = Vec<OptValue<>>{std::move(tmp)};
                }
            }
            else {
                result.emplace(name, target);
            }
            assert(target);
            if (auto b = def.value<bool>()) {
            }
            else if (auto i = def.value<std::int64_t>()) {
            }
        }

        template <class String, class Char, template <class...> class Map, template <class...> class Vec>
        ParseError parse(int& index, int argc, Char** argv,
                         OptionDesc<String, Vec, Map>& desc,
                         OptionSet<String, Vec, Map>& result,
                         ParseFlag flag, Vec<String>* arg = nullptr) {
            using option_t = wrap::shared_ptr<Option<String>>;
            bool nooption = false;
            ParseError ret = ParseError::none;
            bool fatal = false;
            auto parse_all = [&](bool failed) {
                if (failed) {
                    if (any(flag & ParseFlag::failure_opt_as_arg)) {
                        arg->push_back(utf::convert<String>(argv[index]));
                        return true;
                    }
                }
                else {
                    if (any(flag & ParseFlag::parse_all) && arg) {
                        arg->push_back(utf::convert<String>(argv[index]));
                        return true;
                    }
                }
                return false;
            };
            bool has_assign = any(flag & ParseFlag::allow_assign);
            auto found_option = [&](int offset) {
                auto optname = argv[index] + offset;
                option_t opt;
                String name, value, *ptr = nullptr;
                if (has_assign) {
                    auto seq = utils::make_ref_seq(optname);
                    if (helper::read_until(name, seq, "=")) {
                        value = utf::convert<String>(argv[index] + seq.rptr + offset);
                        ptr = &value;
                    }
                    desc.find(name, opt);
                }
                else {
                    desc.find(optname, opt);
                }
                if (opt) {
                    return parse_one(index, argc, argv, opt, result, flag, ptr);
                }
                return ParseError::not_found;
            };
            for (; index < argc; index++) {
                if (nooption || argv[index][0] != '-') {
                    if (parse_all(false)) {
                        continue;
                    }
                    return ParseError::suspend_parse;
                }
                if (helper::equal(argv[index], "--")) {
                    if (any(flag & ParseFlag::ignore_after_two_prefix)) {
                        nooption = true;
                        continue;
                    }
                }
                else if (helper::starts_with(argv[index], "--")) {
                    if (any(flag & ParseFlag::two_prefix_longname)) {
                        if (found_option(2)) {
                            continue;
                        }
                        if (fatal) {
                            return ret;
                        }
                    }
                    if (any(flag & ParseFlag::ignore_not_found)) {
                        continue;
                    }
                    if (parse_all(true)) {
                        continue;
                    }
                    return ParseError::not_found;
                }
                else {
                    if (any(flag & ParseFlag::one_prefix_longname)) {
                        if (found_option(1)) {
                            continue;
                        }
                        if (fatal) {
                            return ret;
                        }
                        if (any(flag & ParseFlag::ignore_not_found)) {
                            continue;
                        }
                        if (parse_all(true)) {
                            continue;
                        }
                        return ParseError::not_found;
                    }
                    if (any(flag & ParseFlag::adjacent_value)) {
                        option_t opt;
                        desc.find(helper::CharView<Char>(argv[index][1]).c_str(), opt);
                        if (opt) {
                            //unimplemented
                            continue;
                        }
                        if (any(flag & ParseFlag::ignore_not_found)) {
                            continue;
                        }
                        if (parse_all(true)) {
                            continue;
                        }
                        return ParseError::not_found;
                    }
                    auto current = index;
                    for (int i = 1; argv[current][i]; i++) {
                        option_t opt;
                        desc.find(helper::CharView<Char>(argv[current][1]).c_str(), opt);
                        if (opt) {
                            //unimplemented
                            continue;
                        }
                        if (any(flag & ParseFlag::ignore_not_found)) {
                            continue;
                        }
                        if (parse_all(true)) {
                            continue;
                        }
                        return ParseError::not_found;
                    }
                }
            }
        }

    }  // namespace cmdline
}  // namespace utils
