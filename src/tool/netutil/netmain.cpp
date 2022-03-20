/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "crtdbg.h"
#include "subcommand.h"
#include "../../include/wrap/argv.h"
#include "../../include/testutil/alloc_hook.h"
using namespace utils;
using namespace cmdline;

wrap::UtfOut& cout = wrap::cout_wrap();

namespace netutil {
    bool* help;
    bool* verbose;
    bool* quiet;

    void common_option(subcmd::RunCommand& ctx) {
        if (help) {
            ctx.option().VarBool(help, "h,help", "show help");
            ctx.option().VarBool(verbose, "v,verbose", "verbose log");
            ctx.option().VarBool(quiet, "quiet", "quiet log");
        }
        else {
            help = ctx.option().Bool("h,help", false, "show help");
            verbose = ctx.option().Bool("v,verbose", false, "verbose log");
            quiet = ctx.option().Bool("quiet", false, "quiet log");
        }
        ctx.set_help_ptr(help);
    }
}  // namespace netutil

int main_help(subcmd::RunCommand& cmd) {
    cout << cmd.Usage(mode);
    return 0;
}

int main_proc(int argc, char** argv) {
    wrap::U8Arg _(argc, argv);
    subcmd::RunContext ctx;
    ctx.Set(argv[0], main_help, "cli network utility", "[command]");
    ctx.SetHelpRun(main_help);
    netutil::common_option(ctx);
    netutil::httpreq_option(ctx);
    netutil::debug_log_option(ctx);
    auto err = subcmd::parse(argc, argv, ctx, mode);
    if (auto msg = error_msg(err)) {
        cout << argv[0] << ": error: " << ctx.erropt() << ": " << msg << "\n";
        return -1;
    }
    return ctx.run();
}

int main(int argc, char** argv) {
    if (argc >= 2 && !helper::equal(argv[1], "mdump")) {
        test::set_log_file("./memdump_by_netutil.txt");
        test::set_alloc_hook(true);
        test::log_hooker = [](test::HookInfo info) {
            if (info.reqid == 162 || info.reqid == 163) {
                return;
            }
        };
    }
    return main_proc(argc, argv);
}
