/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "to_clang.h"

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
        else if (is(expr, "type")) {
            auto type = static_cast<TypeExpr*>(expr);
        }
        else {
            for (auto i = 0; expr->index(i); i++) {
                append_child(node->children, convert_to_node(expr->index(i), scope));
            }
        }
        return node;
    }
}  // namespace minilang
