/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// interface_gen - generate golang like interface

#include "../include/cmdline/make_opt.h"
#include "../include/cmdline/parse.h"

int main(int argc, char** argv) {
    using namespace utils;
    using namespace utils::cmdline;
    DefaultDesc desc;
    desc.set("output-file,o", str_option(""), "set output file", OptFlag::need_value)
        .set("input-file,i", str_option(""), "set input file", OptFlag::need_value);
    int index = 1;
    DefaultSet result;
    utils::wrap::vector<wrap::string> arg;
    parse(index, argc, argv, desc, result, ParseFlag::optget_mode, &arg);
}
