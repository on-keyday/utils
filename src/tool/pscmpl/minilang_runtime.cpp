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
            else if (is(node->expr, "block")) {
                if (length(node->children)) {
                    for (auto ch : node->children->node) {
                        if (!walk_node(ch)) {
                            return false;
                        }
                    }
                }
                return true;
            }
        }

        bool Interpreter::eval_expr(RuntimeValue& value, Node* node) {
            if (is(node->expr, "binary")) {
            }
        }
    }  // namespace runtime
}  // namespace minilang
