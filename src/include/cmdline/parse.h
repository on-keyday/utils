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
#include "../number/number.h"
#include "../number/prefix.h"
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
            require_more_argument,
            not_found,
            invalid_value,
            unexpected_type,
        };

        namespace internal {
            template <template <class...> class Vec, class String, class Char, class Value, class F>
            ParseError parse_value(int& index, int argc, Char** argv,
                                   wrap::shared_ptr<Option<String>>& opt,
                                   ParseFlag flag, String* assign, Value* b,
                                   OptValue<>* target, F&& set_value, bool on_loop = false) {
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
                        if (index + 1 < argc ||
                            (any(opt->flag & OptFlag::no_option_like) && helper::starts_with(argv[index + 1], "-"))) {
                            if (on_loop) {
                                return ParseError::invalid_value;
                            }
                            if (any(opt->flag & OptFlag::need_value)) {
                                return ParseError::need_value;
                            }
                            result = *b;
                            goto SET;
                        }
                        cmp = utf::convert<String>(argv[index + 1]);
                        need_less = true;
                    }
                    if (set_value(result, cmp)) {
                        if (need_less) {
                            index++;
                        }
                    }
                    else if (need_less && !on_loop) {
                        if (any(opt->flag & OptFlag::need_value)) {
                            return ParseError::need_value;
                        }
                        result = *b;
                    }
                    else {
                        return ParseError::invalid_value;
                    }
                }
            SET:
                if (auto v = target->template value<Vec<Value>>()) {
                    v->push_back(std::move(result));
                }
                else if (auto vec = target->template value<Vec<OptValue<>>>()) {
                    vec->push_back(result);
                }
                else {
                    *target = std::move(result);
                }
                return ParseError::none;
            }

            template <template <class...> class Vec, class String, class Char, class Value, class F>
            ParseError parse_vec_value(int& index, int argc, Char** argv,
                                       wrap::shared_ptr<Option<String>>& opt,
                                       ParseFlag flag, String* assign, Vec<Value>* b,
                                       OptValue<>* target, F&& set_value) {
                auto ptr = assign;
                OptValue<> value = Vec<Value>{};
                Vec<Value>* v = value.template value<Vec<Value>>();
                assert(v);
                for (size_t count = 0;; count++) {
                    if (b->size() >= count) {
                        break;
                    }
                    auto e = parse_value<Vec>(index, argc, argv, opt, flag, ptr, &(*b)[count], &value, std::forward<F>(set_value), true);
                    if (e == ParseError::invalid_value) {
                        if (count < opt->minimum) {
                            return ParseError::require_more_argument;
                        }
                        for (auto i = count; count < b->size(); count++) {
                            v->push_back((*b)[count]);
                        }
                        break;
                    }
                    else if (e != ParseError::none) {
                        return e;
                    }
                    ptr = nullptr;
                }
                if (auto vec = target->template value<Vec<OptValue<>>>()) {
                    vec->push_back(std::move(value));
                }
                else {
                    *target = std::move(value);
                }
                return ParseError::none;
            }

            auto judge_bool() {
                return [](auto& result, auto& value) {
                    if (helper::equal("true") || helper::equal("false")) {
                        result = value[0] == 't';
                        return true;
                    }
                    return false;
                };
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_bool(int& index, int argc, Char** argv,
                                  wrap::shared_ptr<Option<String>>& opt,
                                  ParseFlag flag, String* assign, bool* b,
                                  OptValue<>* target) {
                return parse_value(index, argc, argv, opt, flag, assign, b, target, judge_bool());
            }

            auto judge_int() {
                return [](auto& result, auto& value) {
                    size_t offset = 0;
                    int radix = 10;
                    if (auto e = number::has_prefix(value)) {
                        radix = e;
                        offset = 2;
                    }
                    if (number::parse_integer(value, result, radix, offset)) {
                        return true;
                    }
                    return false;
                };
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_int(int& index, int argc, Char** argv,
                                 wrap::shared_ptr<Option<String>>& opt,
                                 ParseFlag flag, String* assign, std::int64_t* b,
                                 OptValue<>* target) {
                return parse_value(index, argc, argv, opt, flag, assign, b, target, judge_int());
            }

            auto judge_string() {
                return [](auto& result, auto& value) {
                    result = value;
                    return true;
                };
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_string(int& index, int argc, Char** argv,
                                    wrap::shared_ptr<Option<String>>& opt,
                                    ParseFlag flag, String* assign, String* b,
                                    OptValue<>* target) {
                return parse_value(index, argc, argv, opt, flag, assign, b, target, judge_string());
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_vec_bool(int& index, int argc, Char** argv,
                                      wrap::shared_ptr<Option<String>>& opt,
                                      ParseFlag flag, String* assign, Vec<bool>* b,
                                      OptValue<>* target) {
                return parse_vec_value(index, argc, argv, opt, flag, assign, b, target, judge_bool());
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_vec_int(int& index, int argc, Char** argv,
                                     wrap::shared_ptr<Option<String>>& opt,
                                     ParseFlag flag, String* assign, Vec<std::int64_t>* b,
                                     OptValue<>* target) {
                return parse_vec_value(index, argc, argv, opt, flag, assign, b, target, judge_int());
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_vec_string(int& index, int argc, Char** argv,
                                        wrap::shared_ptr<Option<String>>& opt,
                                        ParseFlag flag, String* assign, Vec<String>* b,
                                        OptValue<>* target) {
                return parse_vec_value(index, argc, argv, opt, flag, assign, b, target, judge_string());
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
            if (auto b = def.template value<bool>()) {
                return internal::parse_bool<Vec>(index, argc, argv, opt, flag, assign, b, target);
            }
            else if (auto i = def.template value<std::int64_t>()) {
                return internal::parse_int<Vec>(index, argc, argv, opt, flag, assign, i, target);
            }
            else if (auto s = def.template value<String>()) {
                return internal::parse_string<Vec>(index, argc, argv, opt, flag, assign, s, target);
            }
            else if (auto bv = def.template value<Vec<bool>>()) {
                return internal::parse_vec_bool(index, argc, argv, opt, flag, assign, bv, target);
            }
            else if (auto iv = def.template value<Vec<std::int64_t>>()) {
                return internal::parse_vec_int(index, argc, argv, opt, flag, assign, iv, target);
            }
            else if (auto sv = def.template value<Vec<String>>()) {
                return internal::parse_vec_string(index, argc, argv, opt, flag, assign, sv, target);
            }
            return ParseError::unexpected_type;
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
                        auto view = helper::CharView<Char>(argv[index][1]);
                        desc.find(view.c_str(), opt);
                        if (opt) {
                            String* ptr = nullptr;
                            String value;
                            if (argv[index][2]) {
                                value = utf::convert<String>(argv[index] + 2);
                                ptr = &value;
                            }
                            if (auto e = parse_one(index, argc, argv, opt, view.c_str(), result, flag, ptr); e != ParseError::none) {
                                return e;
                            }
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
