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

    auto define_for(auto first_prim, auto prim, auto block) {
        return [=]<class T>(Sequencer<T>& seq, expr::Expr*& expr) {
            auto pos = expr::save_and_space(seq);
            if (!seq.seek_if("for")) {
                return false;
            }
            auto space = expr::bind_space(seq);
            if (!space() && seq.current() != '{') {
                return false;
            }
            pos.ok();
            expr::Expr *bexpr = nullptr, *first = nullptr, *second = nullptr, *third = nullptr;
            auto delall = [&] {
                delete first;
                delete second;
                delete third;
                return false;
            };
            auto make_expr = [&] {
                if (!block(seq, bexpr)) {
                    return delall();
                }
                auto fexpr = new ForExpr{pos.pos};
                fexpr->block = bexpr;
                fexpr->first = first;
                fexpr->second = second;
                fexpr->third = third;
                expr = fexpr;
                return true;
            };
            if (seq.match("{")) {
                return make_expr();
            }
            bool end = false;
            auto bindto = [&](auto& func, auto& e, bool fin = false) {
                if (!func(seq, e)) {
                    return delall();
                }
                space();
                if (seq.match("{")) {
                    end = true;
                    return make_expr();
                }
                if (fin || !seq.match(";")) {
                    return delall();
                }
                return true;
            };
            if (!bindto(first_prim, first) || end) {
                return end;
            }
            if (!bindto(prim, second) || end) {
                return end;
            }
            return bindto(prim, third, true);
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