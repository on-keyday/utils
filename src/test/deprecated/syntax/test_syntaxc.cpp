/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <deprecated/syntax/syntaxc/syntaxc.h>

#include <deprecated/syntax/syntaxc/make_syntaxc.h>
#include <wrap/cout.h>

void test_syntaxc() {
    auto& cout = utils::wrap::cout_wrap();
    utils::syntax::SyntaxC<utils::wrap::string, utils::wrap::vector, utils::wrap::map> test;
    test.cb = [&](auto& ctx) {
        auto& r = ctx.result;
        cout << utils::syntax::keywordv(r.kind) << "-";
        cout << r.token << "-";
        cout << ctx.stack.back();
        cout << "\n";
    };
    auto seq =
        u8R"a(
        ROOT:=JSON
        JSON:="{" OBJPARAM? "}"|"["JSON ["," JSON]*? "]"|NUMBER|STRING|"null"|"true"|"false"
        OBJPARAM:=STRING ":" JSON ["," OBJPARAM]?

        IDVAR:=ID ":=" NUMBER
        EXPR:= ID "+" NUMBER
    )a";

    utils::Sequencer input(seq);

    utils::tokenize::Tokenizer<utils::wrap::string, utils::wrap::vector> tokenizer;

    auto result = test.make_tokenizer(input, tokenizer);

    assert(result && "expect true but make tokenizer failed");

    auto other =
        u8R"(        {
           "hello":"world",
           "hey": [null,"string",0.2,3],
           "google":true,
           "gmail":"com\""
        }
)";

    utils::Sequencer input2(other);

    decltype(tokenizer)::token_t res;

    tokenizer.tokenize(input2, res);

    const char* err = nullptr;

    auto tmp = utils::tokenize::merge(err, res, utils::tokenize::escaped_comment<utils::wrap::string>("\"", "\\"));

    assert(tmp && "failed to merge");

    utils::syntax::Reader<utils::wrap::string> r{res};
    cout << "base:\n"
         << other;
    auto result2 = test.matching(r);

    cout << test.error();
    assert(result2 && "expect true but matching is failed");
}

int main() {
    test_syntaxc();
}
