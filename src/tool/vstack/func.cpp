/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "symbols.h"
#include "func.h"

namespace vstack {
    namespace func {
        bool walk_one(symbol::VstackFn* fn, const std::shared_ptr<token::Token>& cur);

        void add_token(symbol::VstackFn* fn, const std::shared_ptr<token::Token>& cur) {
            if (fn) {
                fn->units.push_back(cur);
            }
        }

        void walk_each(symbol::VstackFn* fn, std::vector<std::shared_ptr<token::Token>>& vec) {
            for (auto& ptr : vec) {
                if (!walk_one(fn, ptr)) {
                    add_token(fn, ptr);
                }
            }
        }

        bool walk_one(symbol::VstackFn* fn, const std::shared_ptr<token::Token>& cur) {
            if (auto br = token::is_Brackets(cur)) {
                if (br->callee) {
                    walk_one(fn, br->center);
                    walk_one(fn, br->callee);
                    add_token(fn, cur);
                    return true;
                }
                else {
                    walk_one(fn, br->center);
                    return false;
                }
            }
            if (auto bi = token::is_BinTree(cur)) {
                walk_one(fn, bi->left);
                walk_one(fn, bi->right);
                return false;
            }
            if (auto un = token::is_UnaryTree(cur)) {
                walk_one(fn, un->target);
                return false;
            }
            if (auto inner_fn = is_Func(cur)) {
                walk_each(&inner_fn->vstack_fn, inner_fn->block->elements);
                return true;  // should avoid add to outer func
            }
            if (auto block = token::is_Block(cur)) {
                walk_each(fn, block->elements);
                return false;
            }
            if (auto let = is_Let(cur)) {
                walk_one(fn, let->init);
                return false;
            }
            return false;
        }

        void walk(const std::shared_ptr<token::Token>& tok) {
            walk_one(nullptr, tok);
        }

    }  // namespace func
}  // namespace vstack
