/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
using namespace utils;
using namespace cmdline;

wrap::UtfOut& cout = wrap::cout_wrap();

namespace netutil {
    bool* help;

    void common_option(subcmd::RunCommand& ctx) {
        if (help) {
            ctx.option().VarBool(help, "h,help", "show help");
        }
        else {
            help = ctx.option().Bool("h,help", false, "show help");
        }
    }
}  // namespace netutil

int main_help(subcmd::RunCommand& cmd) {
    cout << cmd.Usage(mode);
    return 0;
}

int main(int argc, char** argv) {
    subcmd::RunContext ctx;
    ctx.Set(argv[0], main_help, "cli network utility", "[command]");
    netutil::common_option(ctx);
    netutil::httpreq_option(ctx);
    auto err = subcmd::parse(argc, argv, ctx, mode);
    if (auto msg = error_msg(err)) {
        cout << "error: " << ctx.erropt() << ": " << msg << "\n";
        return -1;
    }
    return ctx.run();
}
