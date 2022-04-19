/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// statement - statement expression
#pragma once
#include "expression.h"

namespace utils {
    namespace parser {
        namespace expr {
            struct StatExpr : expr::Expr {
                expr::Expr* block;
                expr::Expr* first;
                expr::Expr* second;
                expr::Expr* third;
                StatExpr(const char* ty, size_t p)
                    : expr::Expr(ty, p) {}

                expr::Expr* index(size_t i) const override {
                    if (i == 0) return block;
                    if (i == 1) return first;
                    if (i == 2) return second;
                    if (i == 3) return third;
                    return nullptr;
                }

                bool stringify(expr::PushBacker pb) const override {
                    helper::append(pb, type_);
                    return true;
                }

                ~StatExpr() {
                    delete block;
                    delete first;
                    delete second;
                    delete third;
                }
            };

            auto define_statement(const char* type, int count, auto first_prim, auto prim, auto block, bool allow_omit = false) {
                return [=]<class T>(Sequencer<T>& seq, expr::Expr*& expr, ErrorStack& stack) {
                    auto pos = expr::save_and_space(seq);
                    if (!seq.seek_if(type)) {
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
                        if (!block(seq, bexpr, stack)) {
                            return delall();
                        }
                        auto fexpr = new StatExpr{type, pos.pos};
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
                    if (count == 0) {
                        return false;
                    }
                    bool end = false;
                    auto bindto = [&](auto& func, auto& e, bool fin = false) {
                        if (!allow_omit || !seq.match(";")) {
                            if (!func(seq, e, stack)) {
                                return delall();
                            }
                        }
                        space();
                        if (seq.match("{")) {
                            end = true;
                            return make_expr();
                        }
                        if (fin || !seq.seek_if(";")) {
                            return delall();
                        }
                        space();
                        return true;
                    };
                    bool fin = false;
                    if (count <= 1) {
                        fin = true;
                    }
                    if (!bindto(first_prim, first, fin) || end) {
                        return end;
                    }
                    if (count <= 2) {
                        fin = true;
                    }
                    if (!bindto(prim, second, fin) || end) {
                        return end;
                    }
                    return bindto(prim, third, true);
                };
            }

            template <class... Fn>
            auto define_statements(Fn... fn) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    bool res = false;
                    size_t start = seq.rptr;
                    auto check = [&](auto& f) {
                        if (!f(seq, expr, stack) && seq.rptr != start) {
                            return true;
                        }
                        if (expr) {
                            res = true;
                            return true;
                        }
                        return false;
                    };
                    auto v = (... || check(fn));
                    if (v == false) {
                        PUSH_ERROR(stack, "statements", "expect statement but not", start, seq.rptr);
                    }
                    return res;
                };
            }

            template <class String, template <class...> class Vec>
            struct BlockExpr : Expr {
                String name;
                Vec<Expr*> exprs;

                BlockExpr(const char* t, size_t pos)
                    : Expr(t, pos) {}

                BlockExpr(size_t pos)
                    : Expr("struct", pos) {}

                Expr* index(size_t index) const override {
                    if (exprs.size() <= index) {
                        return nullptr;
                    }

                    return exprs[index];
                }

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, name);
                    return true;
                }

                ~BlockExpr() {
                    for (auto v : exprs) {
                        delete v;
                    }
                }
            };

            template <class String, template <class...> class Vec, class Fn>
            auto define_block(Fn cond_expr, bool reqname = false, const char* type = "struct", char brackbegin = '{', char brackend = '}') {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    auto space = [&] {
                        helper::space::consume_space(seq, true);
                    };
                    auto pos = save_and_space(seq);
                    String name;
                    if (reqname) {
                        if (!helper::read_whilef<true>(name, seq, [](auto c) {
                                return number::is_alnum(c);
                            })) {
                            return false;
                        }
                    }
                    space();
                    auto reqbrack = brackbegin && brackend;
                    if (!name.size() && reqbrack) {
                        pos.pos = seq.rptr;
                    }
                    auto push_msg = [&](auto brack, bool eof = false) {
                        number::Array<30, char, true> msg{0};
                        helper::append(msg, "expect `");
                        msg.push_back(brack);
                        helper::appends(msg, "` but ", eof ? "reached eof" : "not");
                        PUSH_ERROR(stack, "block", msg.c_str(), pos.pos, seq.rptr)
                    };
                    if (reqbrack && !seq.consume_if(brackbegin)) {
                        push_msg(brackbegin);
                        return false;
                    }
                    Vec<Expr*> vexpr;
                    while (true) {
                        space();
                        if (reqbrack) {
                            if (seq.consume_if(brackend)) {
                                break;
                            }
                            if (seq.eos()) {
                                push_msg(brackend, true);
                                return pos.fatal();
                            }
                        }
                        else {
                            if (seq.eos()) {
                                break;
                            }
                        }
                        if (!cond_expr(seq, expr, stack)) {
                            for (auto v : vexpr) {
                                delete v;
                            }
                            expr = nullptr;
                            return pos.fatal();
                        }
                        vexpr.push_back(expr);
                        expr = nullptr;
                    }
                    auto sexpr = new BlockExpr<String, Vec>{type, pos.pos};
                    sexpr->name = std::move(name);
                    sexpr->exprs = std::move(vexpr);
                    expr = sexpr;
                    return pos.ok();
                };
            }

            template <class String>
            struct LetExpr : Expr {
                LetExpr(const char* tyname, size_t pos)
                    : expr::Expr(tyname, pos) {}
                String idname;
                Expr* type_expr = nullptr;
                Expr* init_expr = nullptr;

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, idname);
                    return true;
                }

                Expr* index(size_t i) const override {
                    if (i == 0) return type_expr;
                    if (i == 1) return init_expr;
                    return nullptr;
                }
            };

            template <class String, class Filter = decltype(default_filter())>
            auto define_vardef(const char* tyname, const char* keyword, auto exp, auto type_, const char* type_sig = ":", const char* init_sig = "=", Filter filter = default_filter()) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    auto pos = save_and_space(seq);
                    auto space = bind_space(seq);
                    if (keyword) {
                        if (!seq.seek_if(keyword)) {
                            return false;
                        }

                        if (!space()) {
                            return false;
                        }
                        pos.ok();
                    }
                    String name;
                    if (!expr::variable(seq, name, seq.rptr, filter)) {
                        PUSH_ERROR(stack, keyword, "expect identifier name but not", pos.pos, seq.rptr)
                        return false;
                    }
                    pos.ok();
                    space();
                    Expr *texpr = nullptr, *eexpr = nullptr;
                    if (type_sig && seq.seek_if(type_sig)) {
                        if (!type_(seq, texpr, stack)) {
                            PUSH_ERROR(stack, keyword, "expect type but not", pos.pos, seq.rptr)
                            return false;
                        }
                        space();
                    }
                    if (init_sig && seq.seek_if("=")) {
                        if (!exp(seq, eexpr, stack)) {
                            PUSH_ERROR(stack, keyword, "expect expr but not", pos.pos, seq.rptr);
                            delete texpr;
                            return false;
                        }
                        space();
                    }
                    auto lexpr = new LetExpr<String>{tyname, pos.pos};
                    lexpr->idname = std::move(name);
                    lexpr->type_expr = texpr;
                    lexpr->init_expr = eexpr;
                    expr = lexpr;
                    return true;
                };
            }

            auto define_return(const char* tyname, const char* keyword, auto exp) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    auto pos = save_and_space(seq);
                    if (!seq.seek_if(keyword)) {
                        return false;
                    }
                    auto space = bind_space(seq);
                    if (!space()) {
                        return false;
                    }
                    pos.ok();
                    if (!exp(seq, expr, stack)) {
                        return false;
                    }
                    auto wexpr = new WrapExpr{tyname, pos.pos};
                    wexpr->child = expr;
                    expr = wexpr;
                    return true;
                };
            };
        }  // namespace expr
    }      // namespace parser
}  // namespace utils
