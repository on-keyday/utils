/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cmdline/subcmd/cmdcontext.h>
#include <wrap/cout.h>

namespace durl {
    namespace subcmd = utils::cmdline::subcmd;
    namespace opt = utils::cmdline::option;
    extern utils::wrap::UtfOut& cout;
    struct GlobalOption {
        bool help = false;

        static int show_usage(subcmd::RunCommand& cmd) {
            cout << cmd.Usage(opt::ParseFlag::assignable_mode);
            return 1;
        }
        void bind(subcmd::RunContext& ctx) {
            ctx.option().VarBool(&help, "h,help", "show this help");
            bind_help(ctx);
            ctx.SetHelpRun(show_usage);
        }
        void bind_help(subcmd::RunCommand& ctx) {
            ctx.set_help_ptr(&help);
        }
    };
}  // namespace durl
