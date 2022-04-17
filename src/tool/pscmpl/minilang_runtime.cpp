/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"

#define NOT_ERROR(expr, err) \
    bool err = false;        \
    if (expr; err) {         \
        return false;        \
    }                        \
    else
#define CHANGE_SCOPE(result, expr)       \
    bool result = false;                 \
    {                                    \
        RuntimeScope scope;              \
        scope.static_scope = node->owns; \
        scope.parent = current;          \
        current = &scope;                \
        result = expr;                   \
        current = scope.parent;          \
    }
namespace minilang {
    namespace runtime {
        bool Interpreter::eval_for(Node* node) {
            auto len = length(node->children);
            auto cond_repeat = [&](Node* inits, Node* cond, Node* rep, Node* block) {
                auto repeat = [&] {
                    while (true) {
                        bool brek = false;
                        auto func = [&] {
                            NOT_ERROR(auto res = !cond || eval_as_bool(cond, err), err) {
                                if (res == false) {
                                    brek = true;
                                    return true;
                                }
                                if (!walk_node(block)) {
                                    return false;
                                }
                            }
                        };
                        CHANGE_SCOPE(res, func())
                        if (!res) {
                            return false;
                        }
                        if (brek) {
                            break;
                        }
                    }
                };
                if (inits) {
                    auto with_init = [&] {
                        if (!walk_node(inits)) {
                            return false;
                        }
                        return repeat();
                    };
                }
                else {
                    return repeat();
                }
                return true;
            };
            if (len == 1) {
                auto block = node->children->node[0];
                return cond_repeat(nullptr, nullptr, nullptr, block);
            }
            else if (len == 2) {
                auto cond = node->children->node[0];
                auto block = node->children->node[1];
                return cond_repeat(nullptr, cond, nullptr, block);
            }
            else if (len == 3) {
                auto inits = node->children->node[0];
                auto cond = node->children->node[1];
                auto block = node->children->node[2];
                return cond_repeat(inits, cond, nullptr, block);
            }
            else if (len == 4) {
                auto inits = node->children->node[0];
                auto cond = node->children->node[1];
                auto rep = node->children->node[2];
                auto block = node->children->node[3];
                return cond_repeat(inits, cond, rep, block);
            }
            return false;
        }

        bool Interpreter::walk_node(Node* node) {
            if (is(node->expr, "for")) {
                return eval_for(node);
            }
            else if (is(node->expr, "program") || is(node->expr, "block")) {
                if (length(node->children)) {
                    for (auto ch : node->children->node) {
                        if (!walk_node(ch)) {
                            return false;
                        }
                    }
                }
                return true;
            }
            return false;
        }

        bool Interpreter::eval_binary(RuntimeValue& value, Node* node) {
            auto bin = static_cast<expr::BinExpr*>(node->expr);
            if (expr::is_logical(bin->op)) {
                if (!eval_expr(value, node->children->node[0])) {
                    return false;
                }

                // short-circuit evaluation
                if (value.get_bool()) {
                    if (bin->op == expr::Op::or_) {
                        value.emplace(Boolean{
                            .node = node,
                            .value = true,
                        });
                        return true;
                    }
                }
                else {
                    if (bin->op == expr::Op::and_) {
                        value.emplace(Boolean{
                            .node = node,
                            .value = false,
                        });
                        return true;
                    }
                }

                if (!eval_expr(value, node->children->node[1])) {
                    return false;
                }
                value.emplace(Boolean{
                    .node = node,
                    .value = value.get_bool(),
                });
                return true;
            }
            else if (expr::is_arithmetic(bin->op)) {
                RuntimeValue other;
                if (!eval_expr(value, node->child(0))) {
                    return false;
                }
                if (!eval_expr(other, node->child(1))) {
                    return false;
                }
                auto filter = value.reflect(other);
                if (bin->op == expr::Op::add) {
                    if (!any(filter & OpFilter::add)) {
                        error("operator + is not supported on here", node);
                        return false;
                    }
                    if (auto i1 = value.as_int()) {
                        auto i2 = other.as_int();
                        value.emplace(Integer{
                            .node = node,
                            .value = *i1 + *i2,
                        });
                    }
                    else {
                        auto s1 = value.as_str();
                        auto s2 = value.as_str();
                        value.emplace(String{
                            .node = node,
                            .value = *s1 + *s2,
                        });
                    }
                    return true;
                }
            }
            else if (bin->op == expr::Op::assign) {
                auto varnode = node->child(0);
                if (!is(varnode->expr, expr::typeVariable)) {
                    error("left of operator = must be variable", varnode);
                    return false;
                }
                auto place = resolve(varnode);
                if (!place) {
                    return false;
                }
                if (!eval_expr(value, node->child(1))) {
                    return false;
                }
                place->value = value;
                return true;
            }
            error("this operator is not supported", node);
            return false;
        }

        bool Interpreter::eval_expr(RuntimeValue& value, Node* node) {
            if (is(node->expr, "binary")) {
                return eval_binary(value, node);
            }
            else if (is(node->expr, expr::typeVariable)) {
                auto var = resolve(node);
                if (!var) {
                    return false;
                }
                value = var->value;
                value.relvar = var;
            }
            else if (is(node->expr, expr::typeString)) {
                auto str = static_cast<expr::StringExpr<wrap::string>*>(node->expr);
                value.emplace(String{
                    .node = node,
                    .value = str->value,
                });
            }
            else if (is(node->expr, expr::typeInt)) {
                auto integer = static_cast<expr::IntExpr*>(node->expr);
                value.emplace(Integer{
                    .node = node,
                    .value = integer->value,
                });
            }
            else if (is(node->expr, expr::typeBool)) {
                auto boolean = static_cast<expr::BoolExpr*>(node->expr);
                value.emplace(Boolean{
                    .node = node,
                    .value = boolean->value,
                });
            }
            else {
                error("unexpected node for expression", node);
                return false;
            }
            return true;
        }

        RuntimeVar* Interpreter::resolve(Node* varnode) {
            auto var = static_cast<expr::VarExpr<wrap::string>*>(varnode->expr);
            if (!varnode->symbol) {
                varnode->symbol = resolve_symbol(current->static_scope, "var", var->name);
                if (!varnode->symbol) {
                    error("symbol not resolved", varnode);
                    return false;
                }
                if (varnode->symbol->kind != ScopeKind::global) {
                    error("unresolved symbol must be global but local symbol is here", varnode);
                    return false;
                }
                varnode->resolved_at = 2;
            }
            auto instance = current->find_var(var->name, varnode->symbol);
            if (!instance) {
                error("no symbol instance found", varnode);
                return false;
            }
            return instance;
        }
    }  // namespace runtime
}  // namespace minilang
