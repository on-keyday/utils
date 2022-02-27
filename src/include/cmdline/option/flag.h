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
#include "../../helper/appender.h"

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
                // use `/` like windows command instead of `-` if only pf_short is set
                use_slash = 0x100,
                // use `:` like windows command instead of `=` if sf_assign is set
                use_colon = 0x200,
                // if sf_assign is set and `=` exists,
                // argument is interpreted as a option name and value
                assign_anyway_val = 0x400,

                default_mode = pf_short | pf_long | parse_all,

                golang_mode = pf_short | sf_assign | parse_all,

                optget_mode = pf_short | pf_long | pf_value | sf_assign | sf_ignore | parse_all,

                optget_ext_mode = optget_mode | assign_anyway_val,

                windows_mode = pf_short | use_slash | use_colon | sf_assign | parse_all,

                assignable_mode = pf_short | pf_long | parse_all | sf_assign,

                assign_val_mode = pf_short | pf_long | parse_all | sf_assign | pf_value | assign_anyway_val,
            };

            DEFINE_ENUM_FLAGOP(ParseFlag)

            template <class Result>
            void get_flag_state(Result& result, ParseFlag flag) {
                bool added = false;
                auto add = [&](auto c) {
                    if (added) {
                        helper::append(result, " | ");
                    }
                    helper::append(result, c);
                    added = true;
                };
                if (flag == ParseFlag::default_mode) {
                    add("default_mode = ");
                    added = false;
                }
                else if (flag == ParseFlag::golang_mode) {
                    add("golang_mode = ");
                    added = false;
                }
                else if (flag == ParseFlag::optget_mode) {
                    add("optget_mode = ");
                    added = false;
                }
                else if (flag == ParseFlag::optget_ext_mode) {
                    add("optget_ext_mode = ");
                    added = false;
                }
                else if (flag == ParseFlag::windows_mode) {
                    add("windows_mode =");
                    added = false;
                }
                else if (flag == ParseFlag::assignable_mode) {
                    add("assignable_mode = ");
                    added = false;
                }
                else if (flag == ParseFlag::assign_val_mode) {
                    add("assign_val_mode = ");
                    added = false;
                }
#define ADD(FLAG)                      \
    if (any(flag & ParseFlag::FLAG)) { \
        add(#FLAG);                    \
    }
                ADD(pf_short)
                ADD(pf_long)
                ADD(pf_value)
                ADD(sf_assign)
                ADD(sf_ignore)
                ADD(parse_all)
                ADD(not_found_arg)
                ADD(not_found_ignore)
                ADD(use_slash)
                ADD(use_colon)
                ADD(assign_anyway_val)
#undef ADD
            }

            template <class Result>
            Result get_flag_state(ParseFlag flag) {
                Result result;
                get_flag_state(result, flag);
                return result;
            }

            enum class FlagType {
                arg,
                ignore,

                pf_one_many,
                pf_one_one,
                pf_one_val,
                pf_one_assign,
                pf_two_one,
                pf_two_assign,

                result_bit = 0x400,
                suspend,
                end_of_arg,

                error_bit = 0x800,
                unknown,
                invalid_arg,
                equal_long_pf,
                equal_short_pf,
                assign_on_short_pf,
                equal_one_pf,

                user_error = 0x1000 | error_bit,
                option_not_found,
                require_more_argument,
                too_many_argument,
                not_accepted,
                type_not_match,
                require_once,
            };

            DEFINE_ENUM_FLAGOP(FlagType)

            BEGIN_ENUM_STRING_MSG(FlagType, error_msg)
            ENUM_STRING_MSG(FlagType::invalid_arg, "invalid argument. this may be error by developer")
            ENUM_STRING_MSG(FlagType::equal_long_pf, "expect string starts with long prefix, but string is equal to long prefix")
            ENUM_STRING_MSG(FlagType::equal_short_pf, "expect string starts with short prefix, but string is equal to short prefix")
            ENUM_STRING_MSG(FlagType::assign_on_short_pf, "assignment operator is not allowed with short prefix")
            ENUM_STRING_MSG(FlagType::equal_one_pf, "expect string starts with short prefix, but string is equal to short prefix")
            ENUM_STRING_MSG(FlagType::unknown, "unknown state")
            ENUM_STRING_MSG(FlagType::user_error, "user defined error. developer must show error reason")
            ENUM_STRING_MSG(FlagType::option_not_found, "option not found")
            ENUM_STRING_MSG(FlagType::require_more_argument, "option requires more argument")
            ENUM_STRING_MSG(FlagType::too_many_argument, "too many argument was provided")
            ENUM_STRING_MSG(FlagType::not_accepted, "argument value is not acceptable")
            ENUM_STRING_MSG(FlagType::type_not_match, "argument ype not matched")
            ENUM_STRING_MSG(FlagType::require_once, "option must be only once in command line")
            END_ENUM_STRING_MSG(nullptr)

            struct CmdParseState {
                FlagType state;
                int index;
                int col;
                int arg_track_index;
                size_t opt_begin;
                size_t opt_end;
                const char* arg;
                const char* val;
                char** argv;
                int argc;
                ParseFlag flag;
                bool err;
                char replaced;
            };

            constexpr const char* get_prefix(ParseFlag flag) {
                return any(ParseFlag::use_slash & flag) ? "/" : "-";
            }

            constexpr const char* get_assignment(ParseFlag flag) {
                return any(ParseFlag::use_colon & flag) ? ":" : "=";
            }

            inline FlagType judge_flag_type(int index, int argc, char** argv, ParseFlag flag) {
                if (index >= argc) {
                    return FlagType::end_of_arg;
                }
                if (index < 0 || argc <= 0 || !argv) {
                    return FlagType::invalid_arg;
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
                auto suspend_or_arg = [&] {
                    if (fall) {
                        return FlagType::arg;
                    }
                    else {
                        return FlagType::suspend;
                    }
                };
                auto contains_assign = [&] {
                    return helper::contains(argv[index], get_assignment(flag));
                };
                auto check_assign = [&](FlagType assign, FlagType ornot) {
                    if (fassign) {
                        if (contains_assign()) {
                            return assign;
                        }
                    }
                    return ornot;
                };
                auto handle_pf_long = [&] {
                    if (equal_("--")) {
                        return FlagType::equal_long_pf;
                    }
                    return check_assign(FlagType::pf_two_assign, FlagType::pf_two_one);
                };
                auto handle_pf_short = [&](FlagType assign, FlagType ornot) {
                    if (fvalue) {
                        bool fassign2 = false;
                        if (any(ParseFlag::assign_anyway_val & flag)) {
                            fassign2 = contains_assign();
                        }
                        else {
                            fassign2 = argv[index][2] == get_assignment(flag)[0];
                        }
                        if (fassign && fassign2) {
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
                            return FlagType::equal_short_pf;
                        }
                        FlagType assign = any(ParseFlag::assign_anyway_val & flag)
                                              ? FlagType::pf_one_assign
                                              : FlagType::assign_on_short_pf;
                        return handle_pf_short(assign, FlagType::pf_one_many);
                    }
                    return suspend_or_arg();
                }
                else if (fshort) {
                    if (helper::starts_with(argv[index], get_prefix(flag))) {
                        if (equal_(get_prefix(flag))) {
                            return FlagType::equal_one_pf;
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

            template <class Val>
            struct SafeVal {
               private:
                Val val;
                bool reserved;

               public:
                constexpr SafeVal() {}
                SafeVal(Val&& val, bool reserved)
                    : val(std::move(val)), reserved(reserved) {}

                SafeVal(SafeVal&& m)
                    : val(std::move(m.val)), reserved(m.reserved) {
                    m.reserved = false;
                }

                SafeVal& operator=(SafeVal&& m) {
                    val = std::move(m.val);
                    reserved = m.reserved;
                    m.reserved = false;
                    return *this;
                }

                template <class T>
                T* get_ptr() {
                    auto ptrdir = val.template type_assert<T>();
                    if (ptrdir) {
                        return ptrdir;
                    }
                    auto ptrindir = val.template type_assert<T*>();
                    if (ptrindir) {
                        return *ptrindir;
                    }
                    return nullptr;
                }

                template <class T>
                bool set_value(T&& t) {
                    if (reserved) {
                        auto ptr = get_ptr<std::remove_cvref_t<T>>();
                        if (!ptr) {
                            return false;
                        }
                        *ptr = std::forward<T>(t);
                    }
                    else {
                        val = std::forward<T>(t);
                    }
                    return true;
                }
            };

        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
