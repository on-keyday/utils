/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

            // void show(string usage_or_err)
            // int then(Option opt,option::Context ctx)
            // opt.bind(ctx) is used
            template <class String>
            int parse_or_err(int argc, char** argv, auto&& opt, auto&& show, auto&& then) {
                option::Context ctx;
                opt.bind(ctx);
                auto err = option::parse_required(argc, argv, ctx, helper::nop, option::ParseFlag::assignable_mode);
                if (perfect_parsed(err) && opt.help) {
                    show(ctx.Usage<String>(option::ParseFlag::assignable_mode, argv[0]));
                    return 1;
                }
                if (auto msg = error_msg(err)) {
                    String buf;
                    helper::appends(buf, "error: ", ctx.erropt(), ": ", msg, "\n");
                    show(std::move(buf));
                    return -1;
                }
                return then(opt, ctx);
            }
        }  // namespace templ
    }      // namespace cmdline
}  // namespace utils
