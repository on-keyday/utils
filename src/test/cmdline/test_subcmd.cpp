/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/subcmd/cmdcontext.h>

using namespace utils::cmdline;
void test_subcommand(int argc, char** argv) {
    subcmd::RunContext ctx;
    ctx.run();
}

int main(int argc, char** argv) {
    test_subcommand(argc, argv);
}