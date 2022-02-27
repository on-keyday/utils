/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parser_loop - parser loop
#pragma once
#include "flag.h"

namespace utils {
    namespace cmdline {
        namespace option {
            inline bool parser_loop(int& index, int& col, int& arg_track_index,
                                    size_t& opt_begin, size_t& opt_end,
                                    FlagType& state, int argc, char** argv, ParseFlag flag) {
                if (col != 0) {
                    if (state == FlagType::pf_one_many) {
                        col++;
                        if (argv[index][col] != 0) {
                            opt_begin = col;
                            opt_end = col + 1;
                            return true;
                        }
                    }
                    col = 0;
                }
                index = arg_track_index;
                state = judge_flag_type(index, argc, argv, flag);
                if (any(state & FlagType::result_bit) || any(state & FlagType::error_bit)) {
                    return false;
                }
                if (state == FlagType::pf_one_many || state == FlagType::pf_one_val) {
                    opt_begin = 1;
                    col = 1;
                    opt_end = 2;
                }
                else if (state == FlagType::pf_one_one || state == FlagType::pf_two_one) {
                    opt_begin = state == FlagType::pf_one_one ? 1 : 2;
                    opt_end = bufsize(argv[index]);
                }
                else if (state == FlagType::pf_one_assign || state == FlagType::pf_two_assign) {
                    opt_begin = state == FlagType::pf_one_assign ? 1 : 2;
                    opt_end = helper::find(argv[index], get_assignment(flag));
                }
                arg_track_index++;
                return true;
            }

            template <class Callback>
            FlagType parse(int argc, char** argv, Callback&& callback, ParseFlag flag = ParseFlag::default_mode, int start_index = 0) {
                FlagType state = FlagType::unknown;
                int index = start_index, col = 0, arg_track_index = start_index;
                size_t begin = 0, end = 0;
                CmdParseState pass;
                auto set_common_param = [&] {
                    pass.argc = argc;
                    pass.argv = argv;
                    pass.flag = flag;
                    pass.state = state;
                    pass.arg_track_index = arg_track_index;
                    pass.index = index;
                    pass.col = col;
                    pass.opt_begin = begin;
                    pass.opt_end = end;
                    pass.arg = nullptr;
                    pass.val = nullptr;
                };
                auto set_arg_track_index = [&] {
                    if (pass.arg_track_index > arg_track_index) {
                        arg_track_index = pass.arg_track_index;
                    }
                };
                auto invoke_callback = [&] {
                    if (!callback(pass)) {
                        if (any(pass.state & FlagType::user_error)) {
                            state = pass.state;
                        }
                        else {
                            state = FlagType::user_error;
                        }
                        return false;
                    }
                    return true;
                };
                while (parser_loop(index, col, arg_track_index,
                                   begin, end, state, argc, argv, flag)) {
                    set_common_param();
                    if (state == FlagType::arg) {
                        pass.arg = argv[index];
                        if (!invoke_callback()) {
                            return state;
                        }
                        continue;
                    }
                    else if (state == FlagType::ignore) {
                        if (!invoke_callback()) {
                            return state;
                        }
                        break;
                    }
                    else if (state == FlagType::pf_one_val) {
                        char buf[2] = {argv[index][col], 0};
                        pass.arg = buf;
                        pass.val = argv[index] + col + 1;
                        if (!invoke_callback()) {
                            return state;
                        }
                        set_arg_track_index();
                    }
                    else if (state == FlagType::pf_one_many) {
                        char buf[2] = {argv[index][col], 0};
                        pass.arg = buf;
                        if (!invoke_callback()) {
                            return state;
                        }
                        set_arg_track_index();
                    }
                    else if (state == FlagType::pf_one_assign || state == FlagType::pf_two_assign) {
                        auto tmp = argv[index][end];
                        argv[index][end] = 0;
                        pass.arg = argv[index] + begin;
                        pass.val = argv[index] + end + 1;
                        if (!invoke_callback()) {
                            return state;
                        }
                        argv[index][end] = tmp;
                        set_arg_track_index();
                    }
                    else if (state == FlagType::pf_one_one || state == FlagType::pf_two_one) {
                        pass.arg = argv[index] + begin;
                        if (!invoke_callback()) {
                            return state;
                        }
                        set_arg_track_index();
                    }
                    else {
                        return FlagType::unknown;
                    }
                }
                return state;
            }

        }  // namespace option
    }      // namespace cmdline
}  // namespace utils