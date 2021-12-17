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
    utils::wrap::ArgvVector<> argv;
    auto v = {"--str=value", "--int=92", "--bool=true"};
    argv.translate(v);

    utils::cmdline::OptionDesc<wrap::string, wrap::vector, wrap::map> desc;
}