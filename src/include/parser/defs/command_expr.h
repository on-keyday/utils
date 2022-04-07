/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// command_expr - command_expr
#pragma once
#include "expression.h"
#include "mnemonic.h"

namespace utils {
    namespace parser {
        namespace expr {
            struct CommandExpr : Expr {
                mnemonic::Command cmd;
                Expr* first;
                Expr* second;

                CommandExpr(size_t pos)
                    : Expr("command", pos) {}

                Expr* index(size_t i) const override {
                    if (i == 0) {
                        return first;
                    }
                    if (i == 1) {
                        return second;
                    }
                    return nullptr;
                }

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, mnemonic::mnemonics[int(cmd)].str);
                    return true;
                }
            };

            template <class Fn>
            auto define_command(Fn cond_expr, mnemonic::Command cmd) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    size_t pos = 0;
                    if (mnemonic::consume(seq, pos, int(cmd)) != cmd) {
                        return false;
                    }
                    if (!cond_expr(seq, expr)) {
                        return false;
                    }
                    Expr* second = nullptr;
                    if (mnemonic::mnemonics[int(cmd)].exprcount >= 2) {
                        if (!cond_expr(seq, second)) {
                            delete expr;
                            expr = nullptr;
                            return false;
                        }
                    }
                    auto cexpr = new CommandExpr{pos};
                    cexpr->cmd = cmd;
                    cexpr->first = expr;
                    cexpr->second = second;
                    expr = cexpr;
                    return true;
                };
            }

            template <class Fn>
            auto define_command_each(Fn cond_expr) {
                auto consume = define_command(cond_expr, mnemonic::Command::consume);
                auto require = define_command(cond_expr, mnemonic::Command::require);
                auto any = define_command(cond_expr, mnemonic::Command::any);
                auto bindany = define_command(cond_expr, mnemonic::Command::bindany);
                auto bind = define_command(cond_expr, mnemonic::Command::bind);
                return [consume, require, any, bindany, bind]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    auto start = seq.rptr;
#define CALL(FUNC)                               \
    if (!FUNC(seq, expr) && start != seq.rptr) { \
        return false;                            \
    }                                            \
    if (expr) {                                  \
        return true;                             \
    }
                    CALL(consume)
                    CALL(require)
                    CALL(any)
                    CALL(bindany)
                    CALL(bind)
                    return false;
                };
#undef CALL
            }

            template <class String, template <class...> class Vec>
            struct StructExpr : Expr {
                String name;
                Vec<Expr*> exprs;

                StructExpr(const char* t, size_t pos)
                    : Expr(t, pos) {}

                StructExpr(size_t pos)
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

                ~StructExpr() {
                    for (auto v : exprs) {
                        delete v;
                    }
                }
            };

            template <class String, template <class...> class Vec, class Fn>
            auto define_set(Fn cond_expr, bool reqname, bool reqbrack = true, const char* type = "struct") {
                auto list = define_command_each(cond_expr);
                return [cond_expr, reqname, type, reqbrack]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    auto start = seq.rptr;
                    auto space = [&] {
                        helper::space::consume_space(seq, true);
                    };
                    space();
                    String name;
                    size_t pos = seq.rptr;
                    if (reqname) {
                        pos = seq.rptr;
                        if (!helper::read_whilef<true>(name, seq, [](auto c) {
                                return number::is_alnum(c);
                            })) {
                            return false;
                        }
                    }
                    space();
                    if (!name.size() && reqbrack) {
                        pos = seq.rptr;
                    }
                    if (reqbrack && !seq.consume_if('{')) {
                        start = seq.rptr;
                        return false;
                    }
                    Vec<Expr*> vexpr;
                    while (true) {
                        space();
                        if (reqbrack) {
                            if (seq.consume_if('}')) {
                                break;
                            }
                            if (seq.eos()) {
                                return false;
                            }
                        }
                        else {
                            if (seq.eos()) {
                                break;
                            }
                        }
                        if (!cond_expr(seq, expr)) {
                            for (auto v : vexpr) {
                                delete v;
                            }
                            expr = nullptr;
                            return false;
                        }
                        vexpr.push_back(expr);
                        expr = nullptr;
                    }
                    auto sexpr = new StructExpr<String, Vec>{type, pos};
                    sexpr->name = std::move(name);
                    sexpr->exprs = std::move(vexpr);
                    expr = sexpr;
                    return true;
                };
            }

            template <class String, template <class...> class Vec, class Fn>
            auto define_command_struct(Fn cond_expr, bool reqname) {
                return define_set<String, Vec>(define_command_each(cond_expr), reqname);
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils
