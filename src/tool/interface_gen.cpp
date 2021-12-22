/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// interface_gen - generate golang like interface

#include "../include/cmdline/make_opt.h"
#include "../include/cmdline/parse.h"

#include "../include/wrap/cout.h"

int main(int argc, char** argv) {
    using namespace utils;
    using namespace utils::cmdline;
    auto& cout = wrap::cout_wrap();
    auto& cerr = wrap::cerr_wrap();
    DefaultDesc desc;
    desc.set("output-file,o", str_option(""), "set output file", OptFlag::need_value, "filename")
        .set("input-file,i", str_option(""), "set input file", OptFlag::need_value, "filename")
        .set("help,h", bool_option(true), "show help", OptFlag::none);
    int index = 1;
    DefaultSet result;
    utils::wrap::vector<wrap::string> arg;
    auto err = parse(index, argc, argv, desc, result, ParseFlag::optget_mode, &arg);
    if (err != ParseError::none) {
        cerr << "error: " << argv[index] << ": " << error_message(err) << "\n"
             << "try `ifacegen -h` for more info\n";
        return -1;
    }
    if (result.is_set("help")) {
        cout << desc.help(argv[0]);
        return 1;
    }
    auto in = result.is_set("input-file");
    auto out = result.is_set("output-file");
    if (!in || !out) {
        cerr << "error: need --input-file and --output-file option\n";
    }
}
