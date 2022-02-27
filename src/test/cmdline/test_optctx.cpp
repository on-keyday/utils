/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/option/optcontext.h>
#include <cmdline/option/parse.h>

void test_optctx(int argc, char** argv) {
    using namespace utils::cmdline;
    option::Context ctx;
    option::parse(argc, argv, ctx, utils::helper::nop, option::ParseFlag::optget_ext_mode);
}