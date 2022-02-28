/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/option/optcontext.h>
#include <cmdline/option/parse.h>
#include <cmdline/option/help.h>
#include <wrap/cout.h>

void test_optctx(int argc, char** argv) {
    using namespace utils::cmdline;
    option::Context ctx;
    auto test = ctx.Bool("test,t", false, "test flag", "");
    auto str = ctx.String<utils::wrap::string>("str,s", "default", "help", "VALUE");
    bool test2 = false;
    ctx.VarBool(&test2, "test2,2", "test flag 2", "");
    auto vec = ctx.VecString("vector,v", 2, "vector", "");
    auto veci = ctx.VecInt("int,i", 2, "int vector", "");
    auto v = option::ParseFlag::optget_ext_mode;
    utils::wrap::cout_wrap() << option::desc<utils::wrap::string>(v, ctx.desc.list);
    auto err = option::parse(argc, argv, ctx, utils::helper::nop, v);
}

int main(int argc, char** argv) {
    test_optctx(argc, argv);
}