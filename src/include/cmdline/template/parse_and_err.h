/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// parse_and_err - parse and error
#pragma once
#include "../option/optcontext.h"
#include "help_option.h"

namespace utils {
    namespace cmdline {
        namespace templ {

            template <class T>
            concept has_args = requires(T t) {
                { t.args };
            };

            // void show(string usage_or_err)
            // int then(Option opt,option::Context ctx)
            // opt.bind(ctx) is used
            template <class String>
            int parse_or_err(int argc, char** argv, auto&& opt, auto&& show, auto&& then, bool disable_error_prefix = false) {
                option::Context ctx;
                opt.bind(ctx);
                option::FlagType err;
                if constexpr (has_args<decltype(opt)>) {
                    err = option::parse_required(argc, argv, ctx, opt.args, option::ParseFlag::assignable_mode);
                }
                else {
                    err = option::parse_required(argc, argv, ctx, helper::nop, option::ParseFlag::assignable_mode);
                }
                if (perfect_parsed(err) && opt.help) {
                    if constexpr (has_args<decltype(opt)>) {
                        show(ctx.Usage<String>(option::ParseFlag::assignable_mode, argv[0], "[option] args..."), false);
                    }
                    else {
                        show(ctx.Usage<String>(option::ParseFlag::assignable_mode, argv[0]), false);
                    }
                    return 1;
                }
                if (auto msg = error_msg(err)) {
                    String buf;
                    if (!disable_error_prefix) {
                        strutil::append(buf, "error: ");
                    }
                    strutil::appends(buf, ctx.erropt(), ": ", msg, "\n");
                    show(std::move(buf), true);
                    return -1;
                }
                return then(opt, ctx);
            }
        }  // namespace templ
    }      // namespace cmdline
}  // namespace utils
