/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/syntax/dispatcher/default_dispatcher.h"
#include "../../include/syntax/dispatcher/filter.h"
#include "../../include/syntax/syntaxc/make_syntaxc.h"
#include "../../include/tokenize/merger.h"
#include "../../include/tokenize/fmt/show_current.h"

#include "../../include/wrap/cout.h"

void test_dispatcher() {
    using namespace utils::syntax;
    DefaultDispatcher disp;
    auto& cout = utils::wrap::cout_wrap();

    auto c = make_syntaxc();
    auto seq = utils::make_ref_seq(R"a(
        COMMENT_TAG:="#"
        ROOT:=EXPR*? EOF
        EXPR:=ASSIGN
        ASSIGN:=EQ ["=" ASSIGN]*?
        EQ:=ADD ["==" ADD]*? 
        ADD:=MUL ["+" MUL]*?
        MUL:=PRIM ["*" PRIM]*?
        PRIM:=INTEGER|ID|"("EXPR")"
    )a");
    auto input = utils::make_ref_seq(R"(
        ( 1 + 1 * (3 == 50) ) == 2
    )");
    auto tok = default_parse(c, seq, input, tknz::sh_comment(), tknz::string());
    assert(tok);

    disp.append([&](auto& ctx) {
            cout << ctx.top() << ":" << ctx.what() << ":" << ctx.token()
                 << "\n";
            return MatchState::succeed;
        })
        .append(
            [&](auto&) {
                cout << "equal\n";
                return MatchState::succeed;
            },
            FilterType::filter | FilterType::check, filter::stack_strict(0, "EQ"));
    auto r = Reader<utils::wrap::string>(tok);
    c->cb = [&](auto& ctx) { return disp(ctx); };
    c->matching(r);
    tknz::fmt::show_current(r, c->error());
    cout << c->error();
}

int main() {
    test_dispatcher();
}
