/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/subcmd/cmdcontext.h>
#include <cmdline/subcmd/parse.h>
#include <wrap/cout.h>

using namespace futils::cmdline;
void test_subcommand(int argc, char** argv) {
    auto& cout = futils::wrap::cout_wrap();
    subcmd::RunContext ctx;
    auto default_runner = [&](subcmd::RunCommand& ctx) {
        for (auto& a : ctx.arg()) {
            cout << "arg: " << a << "\n";
        }
        return 0;
    };
    ctx.Set(
        argv[0], default_runner,
        "subcommand test");
    bool* upper = nullptr;
    auto go = ctx.SubCommand(
        "go", [&](subcmd::RunCommand& ctx) {
            auto v = ctx.option().value<bool>("upper");
            assert(*upper ? v : true);
            if (*upper) {
                cout << "GO GO GO!\n";
            }
            else {
                cout << "go go go!\n";
            }
            default_runner(ctx);
            return 0;
        },
        "go go go!");
    upper = go->option().Bool("upper,u", false, "option");
    ctx.option().VarBool(upper, "upper,u", "upper option");
    ctx.SubCommand(
        "ohno", [&](subcmd::RunCommand& ctx) {
            if (*upper) {
                cout << "OH NO!\n";
            }
            else {
                cout << "oh no!\n";
            }
            default_runner(ctx);
            return 0;
        },
        "oh no!");
    auto err = subcmd::parse(argc, argv, ctx);
    if (auto msg = error_msg(err)) {
        cout << "erorr: " << ctx.erropt() << ": " << msg << "\n";
        return;
    }
    ctx.run();
}

int main(int argc, char** argv) {
    test_subcommand(argc, argv);
}
