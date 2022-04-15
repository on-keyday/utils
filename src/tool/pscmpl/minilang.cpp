/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"

namespace minilang {

    Node* convert_to_node(expr::Expr* expr, Scope* scope, bool root = false) {
        auto node = new Node{};
        node->expr = expr;
        node->belongs = scope;
        node->root = root;
        if (is(expr, "for") || is(expr, "if")) {
            auto block = expr->index(0);
            node->owns = new Scope{};
            for (auto i = 1; expr->index(i); i++) {
                auto cond = expr->index(i);
                auto cond_node = convert_to_node(cond, scope);
                append_child(node->children, cond_node);
            }
            append_child(node->children, convert_to_node(block, node->owns));
        }
        else if (is(expr, "let")) {
            auto let = static_cast<expr::LetExpr<wrap::string>*>(expr);
            append_symbol(scope, node, let->idname);
            if (let->type_expr) {
                append_child(node->children, convert_to_node(let->type_expr, scope));
            }
            if (let->init_expr) {
                append_child(node->children, convert_to_node(let->init_expr, scope));
            }
        }
        else if (is(expr, "variable")) {
        }
        else if (is(expr, "type")) {
            auto texpr = static_cast<TypeExpr*>(expr);
            if (texpr->kind == TypeKind::primitive) {
                node->relate = resolve_symbol(scope, "type", texpr->val);
                if (node->relate) {
                    node->resolved_at = 1;
                }
            }
        }
        else {
            for (auto i = 0; expr->index(i); i++) {
                append_child(node->children, convert_to_node(expr->index(i), scope));
            }
        }
        return node;
    }
}  // namespace minilang
