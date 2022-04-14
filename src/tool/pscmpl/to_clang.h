/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <parser/expr/command_expr.h>
#include <parser/expr/jsoncvt.h>
#include <wrap/light/string.h>
#include <wrap/light/vector.h>
#include <wrap/light/hash_map.h>

namespace minilang {
    using namespace utils;
    namespace expr = utils::parser::expr;

    struct ForExpr : expr::Expr {
        expr::Expr* block;
        expr::Expr* first;
        expr::Expr* second;
        expr::Expr* third;
        ForExpr(size_t p)
            : expr::Expr("for", p) {}
    };

    auto define_for(auto&& fn) {
        return [=]<class T>(Sequencer<T>& seq, expr::Expr*& e) {
        };
    }

    template <class T>
    auto define_minilang(Sequencer<T>& seq, expr::PlaceHolder*& ph) {
        auto rp = expr::define_replacement(ph);
        auto mul = expr::define_binary(
            rp,
            expr::Ops{"*", expr::Op::mul},
            expr::Ops{"/", expr::Op::div},
            expr::Ops{"%", expr::Op::mod});
        auto add = expr::define_binary(
            mul,
            expr::Ops{"+", expr::Op::add},
            expr::Ops{"-", expr::Op::sub});
        auto eq = expr::define_binary(
            add,
            expr::Ops{"==", expr::Op::equal});
        auto and_ = expr::define_binary(
            eq,
            expr::Ops{"&&", expr::Op::and_});
        auto or_ = expr::define_binary(
            and_,
            expr::Ops{"||", expr::Op::or_});
        auto exp = expr::define_assignment(
            or_,
            expr::Ops{"=", expr::Op::assign});
    }
}  // namespace minilang