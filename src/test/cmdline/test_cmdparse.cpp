/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/wrap/argv.h"
#include "../../include/cmdline/parse.h"

#include "../../include/wrap/lite/lite.h"

void test_parse() {
    using namespace utils;
    utils::wrap::ArgvVector<> arg;
    auto v = {"--str=value", "--int=92", "--bool=true"};
    arg.translate(v);
    char** argv;
    int argc;
    arg.arg(argc, argv);

    utils::cmdline::OptionDesc<wrap::string, wrap::vector, wrap::map> desc;
    desc("str", wrap::string(), "help str", cmdline::OptFlag::must_assign);
    utils::cmdline::OptionSet<wrap::string, wrap::vector, wrap::map> result;
    int index = 0;
    utils::cmdline::parse(index, argc, argv, desc, result,
                          cmdline::ParseFlag::allow_assign | cmdline::ParseFlag::two_prefix_longname);
}