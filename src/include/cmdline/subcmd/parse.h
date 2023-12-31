/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
                auto err = option::parse(
                    argc, argv, cur, helper::nop,
                    flag & ~option::ParseFlag::parse_all, start_index);
                if (any(err & option::FlagType::error_bit)) {
                    return err;
                }
                else if (err == option::FlagType::end_of_arg) {
                    if (ctx.need_sub()) {
                        return option::FlagType::require_subcmd;
                    }
                    return err;
                }
                else if (err == option::FlagType::ignore) {
                    if (ctx.need_sub()) {
                        return option::FlagType::require_subcmd;
                    }
                    auto idx = cur.errindex();
                    assert(idx < argc);
                    return option::parse(argc, argv, cur, ctx.arg(), flag, idx + 1);
                }
                else if (err == option::FlagType::suspend) {
                    auto idx = cur.errindex();
                    assert(idx < argc);
                    auto found = ctx.find_cmd(argv[idx]);
                    if (found == ctx.cmd_end()) {
                        if (ctx.need_sub()) {
                            return option::FlagType::subcmd_not_found;
                        }
                        return option::parse(argc, argv, cur, ctx.arg(), flag, idx);
                    }
                    auto& next = *found;
                    return parse(argc, argv, next, flag, idx + 1);
                }
                else {
                    return err;
                }
            }
        }  // namespace subcmd
    }      // namespace cmdline
}  // namespace utils
