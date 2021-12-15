/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse_impl - option parse implementation
#pragma once

#include "cast.h"
#include "option_result.h"
#include "../helper/strutil.h"
#include "../number/number.h"
#include "../number/prefix.h"
#include "../utf/convert.h"

namespace utils {
    namespace cmdline {
        enum class ParseError {
            none,
            not_one_opt,
            not_assigned,
            need_value,
            bool_not_true_or_false,
            int_not_number,
        };

        namespace internal {
            template <class Char, class Opt, class Result, class String, class F>
            ParseError parse_option_common(Opt* opt, int& index, int argc, Char** argv, String* assign, F&& f) {
                auto v = wrap::make_shared<Opt>();
                v->value = opt->value;
                result.value = v;
                bool need_val = any(opt->attr & OptionAttribute::need_value);
                if (any(intopt->attr & OptionAttribute::must_assign)) {
                    if (!assign) {
                        if (need_val) {
                            return ParseError::need_value;
                        }
                        return ParseError::none;
                    }
                }
                if (assign) {
                    if (auto e = f(v, *assign, true, need_val); e != ParseError::none) {
                        return e;
                    }
                }
                else if (index + 1 < argc) {
                    if (auto e = f(v, argv[index + 1], false, need_val); e != ParseError::none) {
                        return e;
                    }
                }
                else if (need_val) {
                    return ParseError::need_value;
                }
                return ParseError::none;
            }

            template <class Char, class String, template <class...> class Vec>
            ParseError parse_booloption(BoolOption<String, Vec>* booopt, int& index, int argc, Char** argv, OptionResult<String, Vec>& result, String* assign) {
                return parse_option_common(booopt, index, argc, argv, assign, [&](auto& v, auto& str, bool is_as, bool need_val) {
                    if (helper::equal(str, "true")) {
                        v->value = true;
                    }
                    else if (helper::equal(str, "false")) {
                        v->value = false;
                    }
                    else if (!is_as) {
                        return ParseError::none;
                    }
                    else {
                        return ParseError::bool_not_true_or_false;
                    }
                    if (!is_as) {
                        index++;
                    }
                });
                /*auto v = wrap::make_shared<BoolOption<String, Vec>>();
                v->value = boopt->value;
                result.value = v;
                bool need_val = any(booopt->attr & OptionAttribute::need_value);
                if (any(booopt->attr & OptionAttribute::must_assign)) {
                    if (!assign) {
                        if (need_val) {
                            return ParseError::need_value;
                        }
                        return ParseError::none;
                    }
                }
                if (assign) {
                    if (helper::equal(*assign, "true")) {
                        v->value = true;
                    }
                    else if (helper::equal(*assign, "false")) {
                        v->value = false;
                    }
                    else {
                        return ParseError::bool_not_true_or_false;
                    }
                }
                else if (index + 1 < argc) {
                    if (helper::equal(argv[index + 1], "true")) {
                        v->value = true;
                        index++;
                    }
                    else if (helper::equal(argv[index + 1], "false")) {
                        v->value = false;
                        index++;
                    }
                    else if (need_val) {
                        return ParseError::need_value;
                    }
                }
                else if (need_val) {
                    return ParseError::need_value;
                }
                return ParseError::none;*/
            }

            template <class Char, class String, template <class...> class Vec>
            ParseError parse_intoption(IntOption<String, Vec>* intopt, int& index, int argc, Char** argv, OptionResult<String, Vec>& result, String* assign) {
                auto v = wrap::make_shared<IntOption<String, Vec>>();
                v->value = intopt->value;
                result.value = v;
                bool need_val = any(booopt->attr & OptionAttribute::need_value);
                if (any(intopt->attr & OptionAttribute::must_assign)) {
                    if (!assign) {
                        if (need_val) {
                            return ParseError::need_value;
                        }
                        return ParseError::none;
                    }
                }
                auto parse_num = [&](auto& value) {
                    int radix = 10;
                    size_t offset = 0;
                    if (auto e = number::has_prefix(value)) {
                        radix = e;
                        offset = 2;
                    }
                    if (!number::parse_integer(value, v->value, radix, offset)) {
                        return ParseError::int_not_number;
                    }
                    return ParseError::none;
                };
                if (assign) {
                    return parse_num();
                }
                else if (index + 1 < argc) {
                    if (parse_num(argv[index + 1]) == ParseError::none) {
                        index++;
                    }
                    else if (need_val) {
                        return ParseError::need_value;
                    }
                }
                else if (need_val) {
                    return ParseError::need_value;
                }
            }

            template <class Char, class String, template <class...> class Vec>
            ParseError parse_stringoption(StringOption<String, Vec>* stropt, int& index, int argc, Char** argv, OptionResult<String, Vec>& result, String* assign) {
                auto v = wrap::make_shared<StringOption<String, Vec>>();
                v->value = stropt->value;
                result.value = v;
                bool need_val = any(booopt->attr & OptionAttribute::need_value);

                if (assign) {
                    v->value = String();
                    utf::convert(*assign, v->value);
                }
                else if (index + 1 < argc) {
                    v->value = String();
                    utf::convert(argv[index + 1], v->value);
                    index++;
                }
                else if (need_val) {
                    return ParseError::need_value;
                }
                return ParseError::none;
            }
        }  // namespace internal

        template <class Char, class OptName, class String, template <class...> class Vec, template <class...> class MultiMap>
        ParseError parse_one(int& index, int argc, Char** argv, const OptName& name,
                             Option<String, Vec>& option,
                             OptionResultSet<String, Vec, MultiMap>& result, String* assign = nullptr) {
            if (any(option.attr & OptionAttribute::once_in_cmd)) {
                if (result.exists(name)) {
                    return ParseError::not_one_opt;
                }
            }
            OptionResult<String, Vec> optres;
            optres.base = std::addressof(option);
            if (BoolOption<String, Vec>* boopt = cast<BoolOption>(&option)) {
                auto e = internal::parse_booloption(boopt, index, argc, argv, optres, assign);
                if (e != ParseError::none) {
                    return e;
                }
            }
            result.emplace(name, std::move(optres));
            return ParseError::none;
        }

        enum class ParseFlag {
            none,
            two_prefix_longname = 0x1,      // option begin with `--` is long name
            allow_assign = 0x2,             // allow `=` operator
            adjacent_value = 0x4,           // `-oValue` become `o` option with value `Value` (default `-oValue` become o,V,a,l,u,e option)
            ignore_after_two_prefix = 0x8,  // after `--` is not option
            one_prefix_longname = 0x10,     // option begin with `-` is long name
            ignore_not_found = 0x20,        // ignore if option is not found
            parse_all = 0x40,               // parse all arg
        };

        DEFINE_ENUM_FLAGOP(ParseFlag)

    }  // namespace cmdline
}  // namespace utils
