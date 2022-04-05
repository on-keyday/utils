/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <parser/defs/expression.h>
#include <wrap/light/string.h>
#include <helper/space.h>
#include <parser/defs/jsoncvt.h>
#include <json/json_export.h>
#include <wrap/cout.h>

using namespace utils::parser;
using namespace utils::json;

void test_expr() {
    auto make_expr = [](auto prim) {
        auto mul = expr::define_binary(
            prim,
            expr::Ops{"*", expr::Op::mul},
            expr::Ops{"/", expr::Op::div},
            expr::Ops{"%", expr::Op::mod});
        auto add = expr::define_binary(
            mul,
            expr::Ops{"+", expr::Op::add},
            expr::Ops{"-", expr::Op::sub});
        auto assign = expr::define_assignment(
            add,
            expr::Ops{"=", expr::Op::assign});
        return assign;
    };
    expr::PlaceHolder* ph;
    auto rp = expr::define_replacement(ph);
    auto assign = make_expr(rp);
    auto seq = utils::make_ref_seq(R"(
        6+(42-0x53)/4
    )");
    auto prim = expr::define_primitive<utils::wrap::string>();
    ph = expr::make_replacement(seq, expr::define_bracket(prim, assign));
    expr::Expr* expr = nullptr;
    assign(seq, expr);
    assert(expr);
    std::int64_t val = 0;
    auto js = convert_to_json<OrderedJSON>(expr);
    utils::wrap::cout_wrap() << to_string<utils::wrap::string>(js, FmtFlag::unescape_slash);
    auto res = expr->as_int(val);
    assert(res == true);
    assert(val == (6 + (42 - 0x53) / 4));
    seq = utils::make_ref_seq(R"(
        name=42-63*2
    )");
    delete expr;
    expr = nullptr;
    assign(seq, expr);
    assert(expr);
    utils::wrap::cout_wrap() << to_string<utils::wrap::string>(convert_to_json<OrderedJSON>(expr), FmtFlag::unescape_slash);
}

int main() {
    test_expr();
}
