/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <deprecated/syntax/make_parser/parse.h>
#include <deprecated/syntax/make_parser/tokenizer.h>
#include <deprecated/tokenize/fmt/show_current.h>

#include <wrap/light/string.h>
#include <wrap/light/map.h>
#include <wrap/cout.h>
#include <wrap/iocommon.h>
using namespace utils;

void test_make_parser() {
    wrap::out_virtual_terminal = true;
    char8_t teststr[] =
        u8R"(
        ROOT:="Hey"  [WHAT*]?!&* ID STRING
        WHAT:="What" [NAME|"hey"]  "?"
        NAME
        :=
        "your""name"
        # HEY:= boke
        )";
    Sequencer input(teststr);
    wrap::shared_ptr<tokenize::Token<wrap::string>> output;
    auto res = syntax::internal::tokenize_and_merge(input, output);
    assert(res && "expect true but tokenize and merge failed");
    wrap::map<wrap::string, utils::wrap::shared_ptr<utils::syntax::Element<wrap::string, wrap::vector>>> result;
    auto ctx = syntax::make_parse_context(output);
    res = syntax::parse<wrap::string, wrap::vector, wrap::map>(ctx, result);
    tokenize::fmt::show_current(ctx.r, ctx.err);
    auto& cout = wrap::cout_wrap();
    cout << ctx.err.pack();
    assert(res && "expect true but parse syntax failed");
    cout << ctx.err.pack("\x1b]0;google\x07", "\x1B[2J\x1B[H");
}

int main() {
    test_make_parser();
}
