/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// command_expr - command_expr
#pragma once
#include "expression.h"
#include "mnemonic.h"
#include "statement.h"

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
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    size_t pos = 0;
                    if (mnemonic::consume(seq, pos, int(cmd)) != cmd) {
                        return false;
                    }
                    if (!cond_expr(seq, expr, stack)) {
                        return false;
                    }
                    Expr* second = nullptr;
                    if (mnemonic::mnemonics[int(cmd)].exprcount >= 2) {
                        if (!cond_expr(seq, second, stack)) {
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
                return define_statements(consume, require, any, bindany, bind, if_);
            }

            template <class String, template <class...> class Vec, class Fn>
            auto define_command_struct(Fn cond_expr, bool reqname, const char* type = "struct") {
                return define_block<String, Vec>(define_command_each(cond_expr), reqname, type);
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils
