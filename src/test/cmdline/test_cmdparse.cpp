/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/wrap/argv.h"
#include "../../include/cmdline/parse.h"
#include "../../include/cmdline/make_opt.h"

#include "../../include/wrap/lite/lite.h"

#include "../../include/wrap/cout.h"

utils::cmdline::ParseError test_parse(std::initializer_list<const char*> v) {
    using namespace utils;
    utils::wrap::ArgvVector<> arg;
    arg.translate(v);
    char** argv;
    int argc;
    arg.arg(argc, argv);

    utils::cmdline::OptionDesc<wrap::string, wrap::vector, wrap::map> desc;
    desc
        .set("str", cmdline::str_option(""), "help str", cmdline::OptFlag::must_assign)
        .set("int", cmdline::int_option(0), "sss", cmdline::OptFlag::must_assign)
        .set("bool", cmdline::bool_option(true), "help str2", cmdline::OptFlag::must_assign)
        .set("bool2", cmdline::bool_option(true), "help", cmdline::OptFlag::none)
        .set("str2", cmdline::str_option("index.html"), "help str", cmdline::OptFlag::need_value | cmdline::OptFlag::no_option_like)
        .set("multi", cmdline::multi_option<wrap::string>(3, 2), "help", cmdline::OptFlag::no_option_like);
    utils::cmdline::OptionSet<wrap::string, wrap::vector, wrap::map> result;
    int index = 0;
    return utils::cmdline::parse(index, argc, argv, desc, result,
                                 cmdline::ParseFlag::allow_assign | cmdline::ParseFlag::two_prefix_longname | cmdline::ParseFlag::one_prefix_longname);
}

int main() {
    using namespace utils;
    auto res = test_parse({"--str=value", "--int=92", "--bool", "--int",
                           "--bool2", "--str2", "value", "-int=3",
                           "--multi", "val1", "val2"});
    assert(res == cmdline::ParseError::none && "expect none but assertion failed");
    res = test_parse({"-int=3en"});
    assert(res == cmdline::ParseError::wrong_assign && "expect wrong_assign but assertion failed");
}
