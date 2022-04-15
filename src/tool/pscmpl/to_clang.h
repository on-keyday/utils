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

    enum class TypeKind {
        primitive,
        ptr,
    };

    struct TypeExpr : expr::Expr {
        TypeExpr(size_t pos)
            : expr::Expr("type", pos) {}
        wrap::string val;
        TypeKind kind;
    };

    auto define_type() {
        return []<class T>(Sequencer<T>& seq, expr::Expr*& expr) {

        };
    }

    template <class T>
    auto define_minilang(Sequencer<T>& seq, expr::PlaceHolder*& ph, expr::PlaceHolder*& ph2) {
        auto rp = expr::define_replacement(ph);
        auto rp2 = expr::define_replacement(ph2);
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
        auto call = expr::define_callexpr<wrap::string, wrap::vector>(exp);
        auto prim = expr::define_primitive(call);
        auto brackets = expr::define_brackets(prim, exp, "brackets");
        auto block = expr::define_block<wrap::string, wrap::vector>(rp2, false, "block");
        auto for_ = expr::define_statement("for", 3, exp, exp, block);
        auto if_ = expr::define_statement("if", 2, exp, exp, block);
        auto stat = expr::define_statements(for_, if_);

        ph = expr::make_replacement(seq, brackets);
        ph2 = expr::make_replacement(seq, stat);
        return expr::define_block<wrap::string, wrap::vector>(stat, false, "program", 0);
    }

    template <class T>
    bool parse(Sequencer<T>& seq, expr::Expr*& expr) {
        expr::PlaceHolder *ph1, *ph2;
        auto parser = define_minilang(seq, ph1, ph2);
        auto res = parser(seq, expr);
        delete ph1;
        delete ph2;
        return res;
    }

    struct Symbols {
    };

    struct Scope {
        Symbols* symbols = nullptr;
    };

    struct NodeChildren;

    struct Node {
        expr::Expr* expr = nullptr;
        Scope* belongs = nullptr;
        Scope* owns = nullptr;
        Node* parent = nullptr;
        NodeChildren* children = nullptr;
        bool root;
    };

    struct NodeChildren {
        wrap::vector<Node*> node;
    };

    void append_child(NodeChildren*& nch, Node* child) {
        if (nch) {
            nch = new NodeChildren{};
        }
        nch->node.push_back(child);
    }
}  // namespace minilang
