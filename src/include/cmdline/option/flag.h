/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// flag - parse flag
#pragma once

#include "../../wrap/lite/enum.h"
#include "../../helper/equal.h"
#include "../../helper/strutil.h"

namespace utils {
    namespace cmdline {
        namespace option {
            enum class ParseFlag {
                // use `-` for one char and string
                // if pf_long is set, this works for only one char
                pf_short = 0x1,
                // use `--` for one char and string
                // if pf_short is set, this works for only string
                pf_long = 0x2,
                // if pf_short is set, one char after `-` is use for option name
                // and remain become option argument
                pf_value = 0x4,
                // if option name include symbol `=`, next string of that become option argument
                sf_assign = 0x8,
                // after `--` become normal arg
                sf_ignore = 0x10,
                // all command line argument will be parsed
                parse_all = 0x20,
                // option not be found will be normal arg
                // this has potential risk similar
                // that programs continue on broken state will make undefined behaviour
                not_found_arg = 0x40,
                // option not be found will be ignored
                // this has potential risk similar
                // that programs continue on broken state will make undefined behaviour
                not_found_ignore = 0x80,
                // use `/` like windows if only pf_short is set
                use_slash = 0x100,
                // use `:` like windows if sf_assign is set
                use_colon = 0x200,
            };

            DEFINE_ENUM_FLAGOP(ParseFlag)

            enum class FlagType {
                invalid,
                arg,
                suspend,
                pf_one_many,
                pf_one_one,
                pf_one_val,
                pf_one_assign,
                pf_two_one,
                pf_two_assign,
                ignore,
            };

            inline FlagType judge_flag_type(int index, int argc, char** argv, ParseFlag flag) {
                if (index >= argc || index < 0 || argc <= 0 || !argv) {
                    return FlagType::invalid;
                }
                auto equal_ = [&](auto str) {
                    return helper::equal(argv[index], str);
                };
                if (any(ParseFlag::sf_ignore & flag) && equal_("--")) {
                    return FlagType::ignore;
                }
                bool fshort = any(ParseFlag::pf_short & flag);
                bool flong = any(ParseFlag::pf_long & flag);
                bool fvalue = any(ParseFlag::pf_value & flag);
                bool fassign = any(ParseFlag::sf_assign & flag);
                bool fall = any(ParseFlag::parse_all & flag);
                const char* one_prefix = "-";
                const char* assign_symbol = "=";
                if (any(ParseFlag::use_slash)) {
                    one_prefix = "/";
                }
                if (any(ParseFlag::use_colon)) {
                    assign_symbol = ":";
                }
                auto suspend_or_arg = [&] {
                    if (fall) {
                        return FlagType::arg;
                    }
                    else {
                        return FlagType::suspend;
                    }
                };
                auto check_assign = [&](FlagType assign, FlagType ornot) {
                    if (fassign) {
                        if (helper::contains(argv[index], assign_symbol)) {
                            return assign;
                        }
                    }
                    return ornot;
                };
                auto handle_pf_long = [&] {
                    if (equal_("--")) {
                        return FlagType::invalid;
                    }
                    return check_assign(FlagType::pf_two_assign, FlagType::pf_two_one);
                };
                auto handle_pf_short = [&](FlagType assign, FlagType ornot) {
                    if (fvalue) {
                        if (argv[index][2] == assign_symbol[0]) {
                            return FlagType::pf_one_assign;
                        }
                        else {
                            return FlagType::pf_one_val;
                        }
                    }
                    else {
                        return check_assign(assign, ornot);
                    }
                };
                if (!fshort && !flong) {
                    return suspend_or_arg();
                }
                if (fshort && flong) {
                    if (helper::starts_with(argv[index], "--")) {
                        return handle_pf_long();
                    }
                    if (helper::starts_with(argv[index], "-")) {
                        if (equal_("-")) {
                            return FlagType::invalid;
                        }
                        return handle_pf_short(FlagType::invalid, FlagType::pf_one_many);
                    }
                    return suspend_or_arg();
                }
                else if (fshort) {
                    if (helper::starts_with(argv[index], one_prefix)) {
                        if (equal_(one_prefix)) {
                            return FlagType::invalid;
                        }
                        return handle_pf_short(FlagType::pf_one_assign, FlagType::pf_one_one);
                    }
                    return suspend_or_arg();
                }
                else {
                    if (helper::starts_with(argv[index], "--")) {
                        return handle_pf_long();
                    }
                    return suspend_or_arg();
                }
            }
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
