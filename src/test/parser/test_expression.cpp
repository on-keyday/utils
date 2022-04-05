/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <parser/defs/expression.h>
#include <wrap/light/string.h>
using namespace utils::parser;

void test_expr() {
    auto prim = expr::define_primitive<utils::wrap::string>();
    auto add = expr::define_binary(
        prim,
        expr::Ops{"+", expr::Op::add},
        expr::Ops{"-", expr::Op::sub});
    auto seq = utils::make_ref_seq(R"(
        6 + 42 - 0x53
    )");
    expr::Expr* expr = nullptr;
    add(seq, expr);
    assert(expr);
    std::int64_t val = 0;
    auto res = expr->as_int(val);
    assert(res == true);
    assert(val == (6 + 42 - 0x53));
}

int main() {
    test_expr();
}
