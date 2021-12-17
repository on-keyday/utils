/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse_impl - implementation of parse
#pragma once
#include "optdef.h"
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
                if (!on_loop && any(opt->flag & OptFlag::must_assign) && !assign) {
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
                else if (auto v = target->template value<Value>()) {
                    auto tmp = std::move(*v);
                    Vec<OptValue<>> to_set;
                    to_set.push_back(std::move(tmp));
                    to_set.push_back(std::move(result));
                    *target = std::move(to_set);
                }
                else {
                    *target = std::move(result);
                }
                return ParseError::none;
            }

            template <template <class...> class Vec, class String, class Char, class Value, class F>
            ParseError parse_vec_value(int& index, int argc, Char** argv,
                                       wrap::shared_ptr<Option<String>>& opt,
                                       ParseFlag flag, String* assign, VecOption<Vec, Value>* b,
                                       OptValue<>* target, F&& set_value) {
                auto ptr = assign;
                OptValue<> value = Vec<Value>{};
                Vec<Value>* v = value.template value<Vec<Value>>();
                assert(v);
                for (size_t count = 0;; count++) {
                    if (b->defval.size() >= count) {
                        break;
                    }
                    auto& ref = b->defval[count];
                    auto e = parse_value<Vec>(index, argc, argv, opt, flag, ptr, &ref, &value, std::forward<F>(set_value), true);
                    if (e == ParseError::invalid_value) {
                        if (count < b->minimum) {
                            return ParseError::require_more_argument;
                        }
                        for (auto i = count; count < b->defval.size(); count++) {
                            v->push_back(b->defval[count]);
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
                else if (auto v = target->template value<Vec<Value>>()) {
                    auto tmp = std::move(*v);
                    Vec<OptValue<>> to_set;
                    to_set.push_back(std::move(tmp));
                    to_set.push_back(std::move(value));
                    *target = std::move(to_set);
                }
                else {
                    *target = std::move(value);
                }
                return ParseError::none;
            }

            auto judge_bool() {
                return [](auto& result, auto& value) {
                    if (helper::equal(value, "true") || helper::equal(value, "false")) {
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
                return parse_value<Vec>(index, argc, argv, opt, flag, assign, b, target, judge_bool());
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
                return parse_value<Vec>(index, argc, argv, opt, flag, assign, b, target, judge_int());
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
                return parse_value<Vec>(index, argc, argv, opt, flag, assign, b, target, judge_string());
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_vec_bool(int& index, int argc, Char** argv,
                                      wrap::shared_ptr<Option<String>>& opt,
                                      ParseFlag flag, String* assign, VecOption<Vec, std::uint8_t>* b,
                                      OptValue<>* target) {
                return parse_vec_value(index, argc, argv, opt, flag, assign, b, target, judge_bool());
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_vec_int(int& index, int argc, Char** argv,
                                     wrap::shared_ptr<Option<String>>& opt,
                                     ParseFlag flag, String* assign, VecOption<Vec, std::int64_t>* b,
                                     OptValue<>* target) {
                return parse_vec_value(index, argc, argv, opt, flag, assign, b, target, judge_int());
            }

            template <template <class...> class Vec, class String, class Char>
            ParseError parse_vec_string(int& index, int argc, Char** argv,
                                        wrap::shared_ptr<Option<String>>& opt,
                                        ParseFlag flag, String* assign, VecOption<Vec, String>* b,
                                        OptValue<>* target) {
                return parse_vec_value(index, argc, argv, opt, flag, assign, b, target, judge_string());
            }
        }  // namespace internal
    }      // namespace cmdline
}  // namespace utils