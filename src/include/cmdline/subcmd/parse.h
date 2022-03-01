/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - subcommand parser
#pragma once
#include "../option/parse.h"

namespace utils {
    namespace cmdline {
        namespace subcmd {
            template <class Ctx>
            option::FlagType parse(int argc, char** argv, Ctx& ctx,
                                   option::ParseFlag flag = option::ParseFlag::default_mode,
                                   int start_index = 1) {
                auto& cur = ctx.context();
                auto err = option::parse(argc, argv, cur, helper::nop, flag, start_index);
            }
        }  // namespace subcmd
    }      // namespace cmdline
}  // namespace utils
