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
            auto define_consume(Fn cond_expr, mnemonic::Command cmd) {
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
        }  // namespace expr
    }      // namespace parser
}  // namespace utils
