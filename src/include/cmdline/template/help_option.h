/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../option/optcontext.h"
#include "../subcmd/cmdcontext.h"

namespace utils {
    namespace cmdline {
        namespace templ {
            struct HelpOption {
                bool help = false;

                void bind_help(option::Context& ctx) {
                    ctx.VarBool(&help, "h,help", "show this help");
                }

                void bind_help(subcmd::RunCommand& cmd) {
                    bind_help(cmd.option());
                    cmd.set_help_ptr(&help);
                }

                void bind(option::Context& ctx) {
                    bind_help(ctx);
                }

                void bind(subcmd::RunCommand& cmd) {
                    bind_help(cmd);
                }
            };
        }  // namespace templ
    }      // namespace cmdline
}  // namespace utils
