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
        };

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
        };

        template <class String, class Char, template <class...> class Map, template <class...> class Vec>
        ParseError parse(int& index, int& col, int argc, Char** argv,
                         OptionDesc<String, Vec, Map>& desc,
                         OptionSet<String, Vec, Map>& result,
                         ParseFlag flag, Vec<String>* arg = nullptr) {
            bool nooption = false;
            for (; index < argc; index++) {
                if (nooption) {
                    if (any(flag & ParseFlag::parse_all) && arg) {
                        String v;
                        utf::convert();
                        arg->push_back();
                    }
                }
                if (helper::equal(argv[index], "--")) {
                    if (any(flag & ParseFlag::ignore_after_two_prefix)) {
                        nooption = true;
                        continue;
                    }
                    if (any(flag & ParseFlag::parse_all)) {
                    }
                }
                else if (helper::starts_with(argv[index], "--")) {
                    if (any(flag & ParseFlag::two_prefix_longname)) {
                    }
                }
                else if (helper::starts_with(argv[index], "-")) {
                }
            }
        }

    }  // namespace cmdline
}  // namespace utils