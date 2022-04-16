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
#define CHANGE_SCOPE(result, expr) \
    bool result = false;           \
    {                              \
        RuntimeScope scope;        \
        scope.relate = node->owns; \
        scope.parent = current;    \
        current = &scope;          \
        result = expr;             \
        current = scope.parent;    \
    }
namespace minilang {
    bool Interpreter::eval_for(Node* node) {
        auto len = length(node->children);
        if (len == 1) {
            auto block = node->children->node[0];
            CHANGE_SCOPE(res, walk_node(block));
            return res;
        }
        else if (len == 2) {
            auto cond = node->children->node[0];
            auto block = node->children->node[1];
            while (true) {
                NOT_ERROR(auto res = eval_as_bool(cond, err), err) {
                    if (res == false) {
                        break;
                    }
                    CHANGE_SCOPE(walk, walk_node(block));
                    if (!walk) {
                        return false;
                    }
                }
            }
            return true;
        }
        else if (len == 3) {
            auto cond = node->children->node[0];
        }
        return false;
    }

    bool Interpreter::walk_node(Node* node) {
        if (is(node->expr, "for")) {
            return eval_for(node);
        }
    }
}  // namespace minilang
