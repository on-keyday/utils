/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/subcmd/cmdcontext.h>
#include <cmdline/subcmd/parse.h>
#include <wrap/cout.h>

using namespace utils::cmdline;
void test_subcommand(int argc, char** argv) {
    auto& cout = utils::wrap::cout_wrap();
    subcmd::RunContext ctx;
    ctx.Set(
        argv[0], [&](subcmd::RunCommand& ctx) {
            for (auto& a : ctx.arg()) {
                cout << "arg: " << a;
            }
            return 0;
        },
        "subcommand test");
    auto err = subcmd::parse(argc, argv, ctx);
    if (auto msg = error_msg(err)) {
        cout << "erorr: " << ctx.get_option().erropt() << ": " << msg << "\n";
        return;
    }
    ctx.run();
}

int main(int argc, char** argv) {
    test_subcommand(argc, argv);
}