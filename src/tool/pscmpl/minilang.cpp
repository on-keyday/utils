/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"

namespace minilang {

    Node* convert_to_node(expr::Expr* expr, Scope* scope, bool root) {
        auto node = new Node{};
        node->expr = expr;
        node->belongs = scope;
        node->root = root;
        auto append_each = [&](int start = 0) {
            for (auto i = start; expr->index(i); i++) {
                append_child(node->children, convert_to_node(expr->index(i), scope));
            }
        };
        if (is(expr, "for") || is(expr, "if")) {
            auto block = expr->index(0);
            node->owns = child_scope(scope, ScopeKind::local);
            auto ptr = static_cast<expr::StatExpr*>(expr);
            if (ptr->first) {
                append_child(node->children, convert_to_node(ptr->first, node->owns));
            }
            if (ptr->second) {
                if (!ptr->first) {
                    append_child(node->children, nullptr);
                }
                append_child(node->children, convert_to_node(ptr->second, node->owns));
            }
            if (ptr->third) {
                if (!ptr->second) {
                    append_child(node->children, nullptr);
                }
                else if (!ptr->first) {
                    append_child(node->children, nullptr);
                    append_child(node->children, nullptr);
                }
                append_child(node->children, convert_to_node(ptr->third, node->owns));
            }
            append_child(node->children, convert_to_node(block, node->owns));
        }
        else if (is(expr, "let")) {
            auto let = static_cast<expr::LetExpr<wrap::string>*>(expr);
            if (let->type_expr) {
                append_child(node->children, convert_to_node(let->type_expr, scope));
            }
            if (let->init_expr) {
                append_child(node->children, convert_to_node(let->init_expr, scope));
            }
            append_symbol(scope, node, "var", let->idname);
        }
        else if (is(expr, "typedef")) {
            auto tdef = static_cast<expr::LetExpr<wrap::string>*>(expr);
            if (tdef->type_expr) {
                append_child(node->children, convert_to_node(tdef->type_expr, scope));
            }
            if (tdef->init_expr) {
                append_child(node->children, convert_to_node(tdef->init_expr, scope));
            }
            append_symbol(scope, node, "type", tdef->idname);
        }
        else if (is(expr, expr::typeVariable)) {
            auto vexpr = static_cast<expr::VarExpr<wrap::string>*>(expr);
            node->symbol = resolve_symbol(scope, "var", vexpr->name);
            if (node->symbol) {
                node->resolved_at = 1;
            }
        }
        else if (is(expr, expr::typeCall)) {
            auto cexpr = static_cast<expr::CallExpr<wrap::string, wrap::vector>*>(expr);
            node->symbol = resolve_symbol(scope, "var", cexpr->name);
            if (node->symbol) {
                node->resolved_at = 1;
            }
            append_each();
        }
        else if (is(expr, "type")) {
            auto texpr = static_cast<TypeExpr*>(expr);
            if (texpr->kind == TypeKind::primitive) {
                node->symbol = resolve_symbol(scope, "type", texpr->val);
                if (node->symbol) {
                    node->resolved_at = 1;
                }
            }
            append_each();
        }
        else {
            append_each();
        }
        return node;
    }

}  // namespace minilang
