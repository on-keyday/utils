/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parsers - standard parsers
#pragma once

#include "parser_loop.h"
#include "../../helper/pushbacker.h"

namespace utils {
    namespace cmdline {
        namespace option {

            template <class Desc, class Result, class Arg>
            auto parse_default_proc(Desc& desc, Result& result, Arg& arg) {
                return [&](option::CmdParseState& state) {
                    auto set_err = [&](FlagType err) {
                        state.state = err;
                        result.index = state.index;
                        result.erropt = state.arg;
                    };
                    auto set_user_err = [&]() {
                        set_err(any(state.state & FlagType::user_error) ? state.state
                                                                        : FlagType::not_accepted);
                    };
                    auto push_current = [&] {
                        if (state.state == FlagType::pf_one_many) {
                            return;
                        }
                        auto v = state.argv[state.index];
                        if (state.replaced) {
                            v[state.opt_end] = state.replaced;
                        }
                        arg.push_back(state.argv[state.index]);
                    };
                    if (state.err) {
                        result.index = state.index;
                        result.erropt = state.argv[state.index];
                        return false;
                    }
                    else if (state.state == FlagType::arg) {
                        arg.push_back(state.arg);
                        return true;
                    }
                    else if (state.state == FlagType::ignore) {
                        for (auto i = state.arg_track_index; i < state.argc; i++) {
                            arg.push_back(state.arg);
                        }
                        return true;
                    }
                    else {
                        auto found = desc.desc.find(state.arg);
                        if (found == desc.desc.end()) {
                            if (any(state.flag & ParseFlag::not_found_arg)) {
                                push_current();
                                return true;
                            }
                            else if (any(state.flag & ParseFlag::not_found_ignore)) {
                                return true;
                            }
                            set_err(FlagType::option_not_found);
                            return false;
                        }
                        auto option = std::get<1>(*found);
                        auto reserved = result.reserved.find(option->mainname);
                        if (reserved != result.reserved.end()) {
                            auto& place = std::get<1>(*reserved);
                            if (!option->parser.parse(place.value, state, true, place.set_count)) {
                                set_user_err();
                                return false;
                            }
                            place.set_count++;
                            return true;
                        }
                        result.result.push_back({});
                        auto& added = result.result.back();
                        added.as_name = state.arg;
                        added.desc = option;
                        if (!option->parser.parse(added.value, state, false, 1)) {
                            set_user_err();
                            return false;
                        }
                        added.set_count = 1;
                        return true;
                    }
                };
            }

            template <class Desc, class Result, class Arg = decltype(helper::nop)>
            FlagType parse(int argc, char** argv,
                           Desc& desc, Result& result, Arg& arg = helper::nop,
                           ParseFlag flag = ParseFlag::default_mode, int start_index = 1) {
                auto proc = parse_default_proc(desc, result, arg);
                auto err = parse(argc, argv, flag, start_index, proc);
                return err;
            }
            template <class Ctx, class Arg = decltype(helper::nop)>
            FlagType parse(int argc, char** argv, Ctx& ctx, Arg& arg = helper::nop,
                           ParseFlag flag = ParseFlag::default_mode, int start_index = 1) {
                return parse(argc, argv, ctx.desc, ctx.result, arg, flag, start_index);
            }
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
