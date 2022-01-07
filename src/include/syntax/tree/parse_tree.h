/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse_tree - tree parser
#pragma once
#include "../matching/context.h"
#include "../matching/state.h"

namespace utils {
    namespace syntax {
        namespace tree {
            template <class T>
            struct TreeType {
                T value;
                TreeType* left;
                TreeType* right;
            };
        }  // namespace tree

        template <class Manager, class String, template <class...> class Vec>
        struct TreeMatcher {
            using tree_t = Manager::tree_t;
            Manager manager;
            tree_t current;
            MatchState operator()(MatchContext<String, Vec>& result) {
                if (result.kind() == KeyWord::literal_symbol) {
                    auto tok = manager.make_tree(result.kind(), result.token());
                }
            }
        };

        template <class String, template <class...> class Vec>
        struct Expr {
        };
    }  // namespace syntax
}  // namespace utils
