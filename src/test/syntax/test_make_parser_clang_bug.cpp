/*license*/

#include "../../include/syntax/make_parser/parse.h"
#include "../../include/syntax/make_parser/tokenizer.h"
#include "../../include/tokenize/fmt/show_current.h"

#include "../../include/wrap/lite/string.h"
#include "../../include/wrap/lite/map.h"
#include "../../include/wrap/cout.h"
using namespace utils;

void test_make_parser() {
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
    auto res = syntax::tokenize_and_merge(input, output);
    assert(res && "expect true but tokenize and merge failed");
    wrap::map<wrap::string, utils::wrap::shared_ptr<utils::syntax::Element<wrap::string, wrap::vector>>> result;
    auto ctx = syntax::make_parse_context(output);
    res = syntax::parse<wrap::string, wrap::vector, wrap::map>(ctx, result);
    tokenize::fmt::show_current(ctx.r, ctx.err);
    auto& cout = wrap::cout_wrap();
    cout << ctx.err.pack();
    assert(res && "expect true but parse syntax failed");
}

int main() {
    test_make_parser();
}
