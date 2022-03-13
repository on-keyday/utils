/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/cmdline/subcmd/cmdcontext.h"
#include "../../include/wrap/cout.h"
using namespace utils;
using namespace cmdline;

constexpr auto mode = option::ParseFlag::assignable_mode;
static auto& cout = wrap::cout_wrap();

int main_help(subcmd::RunCommand& cmd) {
    cout << cmd.Usage(mode);
    return 0;
}

int main(int argc, char** argv) {
    subcmd::RunContext ctx;
    ctx.Set(argv[0], main_help, "cli network utility", "[command]");
    auto err = subcmd::parse(argc, argv, ctx, mode);
    if (auto msg = error_msg(err)) {
        cout << "error: " << ctx.erropt() << ": " << msg << "\n";
        return -1;
    }
    return ctx.run();
}
