/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <deprecated/syntax/tree/parse_tree.h>
#include <deprecated/syntax/syntaxc/make_syntaxc.h>
#include <deprecated/syntax/dispatcher/default_dispatcher.h>

void test_tree() {
    using namespace utils;
    auto c = syntax::make_syntaxc();
    auto def = make_ref_seq(R"a(
        ROOT:=EXPR
        EXPR:=ADD
        ADD:=BOS MUL [["+"|"-"] ADD]*? EOS
        MUL:=BOS PRIM [["*"|"/"] PRIM]*? EOS
        PRIM:=BOS [INTEGER|ID|"("EXPR")"] EOS
    )a");
    auto input = make_ref_seq(R"(
        (a + b) - 3 * 4 + 4
    )");
    auto e = syntax::default_parse(c, def, input);
    assert(e);
    auto r = syntax::make_reader(e);
    namespace st = syntax::tree;
    st::TreeMatcher<> matcher;
    syntax::DefaultDispatcher disp;
    disp.append(&matcher, utils::syntax::FilterType::check);
    c->cb = [&](auto& c) {
        return disp(c);
    };
    c->matching(r);
}

int main() {
    test_tree();
}
