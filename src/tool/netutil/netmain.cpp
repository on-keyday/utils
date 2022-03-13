/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/wrap/argv.h"
using namespace utils;
using namespace cmdline;

wrap::UtfOut& cout = wrap::cout_wrap();

namespace netutil {
    bool* help;
    bool* verbose;

    void common_option(subcmd::RunCommand& ctx) {
        if (help) {
            ctx.option().VarBool(help, "h,help", "show help");
            ctx.option().VarBool(verbose, "v,verbose", "verbose log");
        }
        else {
            help = ctx.option().Bool("h,help", false, "show help");
            verbose = ctx.option().Bool("v,verbose", false, "verbose log");
        }
    }
}  // namespace netutil

int main_help(subcmd::RunCommand& cmd) {
    cout << cmd.Usage(mode);
    return 0;
}

int main(int argc, char** argv) {
    wrap::U8Arg _(argc, argv);
    subcmd::RunContext ctx;
    ctx.Set(argv[0], main_help, "cli network utility", "[command]");
    netutil::common_option(ctx);
    netutil::httpreq_option(ctx);
    auto err = subcmd::parse(argc, argv, ctx, mode);
    if (auto msg = error_msg(err)) {
        cout << argv[0] << ": error: " << ctx.erropt() << ": " << msg << "\n";
        return -1;
    }
    return ctx.run();
}
