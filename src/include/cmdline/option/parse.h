/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parsers - standard parsers
#pragma once

#include "option_set.h"
#include "parser_loop.h"
#include "../../helper/pushbacker.h"

namespace utils {
    namespace cmdline {
        namespace option {
            template <class Arg = decltype(helper::nop)>
            FlagType parse(int argc, char** argv,
                           Description& desc, Results& result,
                           ParseFlag flag = ParseFlag::default_mode, int start_index = 1,
                           Arg& arg = helper::nop) {
                return parse(
                    argc, argv,
                    [&](option::CmdParseState& state) {
                        if (state.err) {
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
                            if (found != desc.desc.end()) {
                                state.state = FlagType::option_not_found;
                                return false;
                            }
                        }
                    },
                    flag, start_index);
            }
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils