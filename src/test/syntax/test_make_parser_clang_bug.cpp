/*license*/

#include "../../include/syntax/make_parser/parse.h"
#include "../../include/syntax/make_parser/tokenizer.h"

#include "../../include/wrap/lite/string.h"
#include "../../include/wrap/lite/map.h"
using namespace utils;

void test_make_parser() {
    char8_t teststr[] =
        u8R"(
        ROOT:="Hey"  [WHAT*]?
        WHAT:="What" [NAME|"hey"]  "?"
        NAME:="your""name"
        # HEY:= boke
    )";
    Sequencer input(teststr);
    wrap::shared_ptr<tokenize::Token<wrap::string>> output;
    auto res = syntax::tokenize_and_merge(input, output);
    assert(res && "expect true but tokenize and merge failed");
    wrap::map<wrap::string, utils::wrap::shared_ptr<utils::syntax::Element<wrap::string, wrap::vector>>> result;
    auto ctx = syntax::make_parse_context(output);
    res = syntax::parse(ctx, result);
    assert(res && "expect true but parse syntax failed");
}

int main() {
    test_make_parser();
}
