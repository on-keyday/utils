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
                auto if_ = define_command(cond_expr, mnemonic::Command::if_);
                return [consume, require, any, bindany, bind, if_]<class T>(Sequencer<T>& seq, Expr*& expr) {
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
                    CALL(if_)
                    return false;
                };
#undef CALL
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
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr) {
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
                    if (reqbrack && !seq.consume_if('{')) {
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
                                return pos.fatal();
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

            template <class String, template <class...> class Vec, class Fn>
            auto define_command_struct(Fn cond_expr, bool reqname, const char* type = "struct") {
                return define_block<String, Vec>(define_command_each(cond_expr), reqname, type);
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils
