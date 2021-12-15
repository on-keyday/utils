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
            using option_t = wrap::shared_ptr<Option<String>>;
            bool nooption = false;
            auto parse_all = [&] {
                if (any(flag & ParseFlag::parse_all) && arg) {
                    arg->push_back(utf::convert<String>(argv[index]));
                    return true;
                }
                return false;
            };
            bool has_assign = any(flag & ParseFlag::allow_assign);
            for (; index < argc; index++) {
                if (nooption) {
                    if (parse_all()) {
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
                        auto optname = argv[index] + 2;
                        if (has_assign) {
                            String name, value;
                            auto seq = utils::make_ref_seq(optname);
                            if (helper::read_until(name, seq, "=")) {
                                value = utf::convert<String>(argv[index] + seq.rptr + 2);
                            }
                        }
                        if (option_t opt; desc.find(optname, opt)) {
                        }
                    }
                    if (parse_all()) {
                        continue;
                    }
                }
                else if (helper::starts_with(argv[index], "-")) {
                }
            }
        }

    }  // namespace cmdline
}  // namespace utils