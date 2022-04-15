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
        array,
    };

    struct TypeExpr : expr::Expr {
        TypeExpr(TypeKind k, size_t pos)
            : kind(k), expr::Expr("type", pos) {}
        wrap::string val;
        TypeKind kind;
        expr::Expr* next = nullptr;
        expr::Expr* expr = nullptr;

        expr::Expr* index(size_t i) const override {
            if (i == 0) return next;
            if (i == 1) return expr;
            return nullptr;
        }

        bool stringify(expr::PushBacker pb) const override {
            helper::append(pb, val);
            return true;
        }
    };

    auto define_type(auto exp) {
        auto fn = [=]<class T>(auto& self, Sequencer<T>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
            auto pos = expr::save_and_space(seq);
            auto space = expr::bind_space(seq);
            auto recall = [&]() {
                return self(self, seq, expr, stack);
            };
            TypeExpr* texpr = nullptr;
            auto make_type = [&](TypeKind kind) {
                texpr = new TypeExpr{kind, pos.pos};
                texpr->next = expr;
            };
            if (seq.seek_if("*")) {
                if (!recall(TypeKind::ptr)) {
                    return false;
                }
                make_type();
                texpr->val = "*";
            }
            else if (seq.seek_if("[")) {
                space();
                if (!seq.match("]")) {
                    if (!exp(seq, expr, stack)) {
                        PUSH_ERROR(stack, "type", "expect expr but error occurred", pos.pos, pos.pos)
                        return false;
                    }
                }
                expr::Expr* rec = expr;
                expr = nullptr;
                if (!seq.seek_if("]")) {
                    delete rec;
                    PUSH_ERROR(stack, "type", "expect `]` but not", pos.pos, seq.rptr);
                    return false;
                }
                if (!recall()) {
                    delete rec;
                    return false;
                }
                make_type(TypeKind::array);
                texpr->expr = rec;
                texpr->val = "[]";
            }
            else {
                wrap::string name;
                if (!expr::variable(seq, name, pos.pos)) {
                    PUSH_ERROR(stack, "type", "expect identifier but not", pos.pos, pos.pos)
                    return false;
                }
                make_type(TypeKind::primitive);
                texpr->val = std::move(name);
            }
            expr = texpr;
            return true;
        };
        return [fn]<class T>(Sequencer<T>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
            return fn(fn, seq, expr, stack);
        };
    }

    struct LetExpr : expr::Expr {
        LetExpr(size_t pos)
            : expr::Expr("let", pos) {}
        wrap::string idname;
        expr::Expr* type_expr = nullptr;
        expr::Expr* init_expr = nullptr;
    };

    auto define_let(auto exp, auto type_) {
        return [=]<class T>(Sequencer<T>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
            auto pos = expr::save_and_space(seq);
            if (seq.seek_if("let")) {
                return false;
            }
            auto space = expr::bind_space(seq);
            if (!space()) {
                return false;
            }
            pos.ok();
            wrap::string name;
            if (!expr::variable(seq, name, seq.pos)) {
                PUSH_ERROR(stack, "let", "expect identifier name but not", pos.pos, seq.rptr)
                return false;
            }
            space();
            expr::Expr *texpr = nullptr, eexpr = nullptr;
            if (seq.seek_if(":")) {
                if (!type_(seq, texpr, stack)) {
                    PUSH_ERROR(stack, "let", "expect type but not", pos.pos, seq.rptr)
                    return false;
                }
                space();
            }
            if (seq.seek_if("=")) {
                if (!exp(seq, eexpr, stack)) {
                    PUSH_ERROR(stack, "let", "expect expr but not", pos.pos, seq.rptr);
                    delete texpr;
                    return false;
                }
                space();
            }
            auto lexpr = new LetExpr{pos.pos};
            lexpr->idname = std::move(name);
            lexpr->type_expr = texpr;
            lexpr->init_expr = eexpr;
            expr = lexpr;
            return true;
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
        auto type_ = define_type(exp);
        auto let = define_let(exp, type_);
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
