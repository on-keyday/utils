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
            };

            template <class Fn>
            auto define_command(Fn cond_expr, mnemonic::Command cmd) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    if (mnemonic::consume(seq, int(cmd)) != cmd) {
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
                    auto cexpr = new CommandExpr{};
                    cexpr->cmd = cmd;
                    cexpr->first = expr;
                    cexpr->second = second;
                    expr = cexpr;
                    return true;
                };
            }

            template <class Fn>
            auto define_command_list(Fn cond_expr) {
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
        }  // namespace expr
    }      // namespace parser
}  // namespace utils
