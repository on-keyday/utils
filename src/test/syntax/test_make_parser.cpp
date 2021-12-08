/*license*/

#include "../../include/syntax/make_parser/parse.h"
#include "../../include/syntax/make_parser/tokenizer.h"

#include "../../include/wrap/lite/string.h"

using namespace utils;

void test_make_parser() {
    char8_t teststr[] =
        u8R"(
        ROOT:="Hey"  [WHAT*]?
        WHAT:="What" [NAME|"hey"]  "?"
        NAME:="your""name"
        #HEY:= boke
    )";
    Sequencer input(teststr);
    wrap::shared_ptr<tokenize::Token<wrap::string>> output;
    auto res = syntax::tokenize_and_merge(input, output);
    assert(res && "expect true but tokenize and merge failed");
}
