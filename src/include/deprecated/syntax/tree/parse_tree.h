/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse_tree - tree parser
#pragma once
#include "../matching/context.h"
#include "../matching/state.h"
#include <helper/equal.h>
#include <helper/readutil.h>

#include <wrap/light/string.h>
#include <wrap/light/vector.h>

namespace utils {
    namespace syntax {
        namespace tree {
            template <class T>
            struct TreeType {
                T value;
                TreeType* left = nullptr;
                TreeType* right = nullptr;
            };

            template <class T>
            struct DefaultManager {
                using tree_t = TreeType<T>*;
                tree_t make_tree(KeyWord kw, const T& v) {
                    return new TreeType<T>{v};
                }
                bool ignore(const T& t) {
                    if (t.size() == 0) {
                        return true;
                    }
                    return helper::equal(t, "(") || helper::equal(t, ")");
                }
            };

            template <class Tree>
            struct StackObj {
                Tree tree = nullptr;
            };

            template <class String = wrap::string, template <class...> class Vec = wrap::vector, class Manager = DefaultManager<String>>
            struct TreeMatcher {
                using tree_t = typename Manager::tree_t;
                Manager manager;
                tree_t current = nullptr;
                Vec<StackObj<tree_t>> stack;
                MatchState operator()(MatchContext<String, Vec>& result, bool cancel) {
                    if (result.kind() == KeyWord::bos) {
                        stack.push_back({.tree = current});
                        current = nullptr;
                        return MatchState::succeed;
                    }
                    else if (result.kind() == KeyWord::eos) {
                        if (stack.back().tree) {
                            auto e = stack.back().tree;
                            e->right = current;
                            current = e;
                        }
                        stack.pop_back();
                        return MatchState::succeed;
                    }
                    if (manager.ignore(result.token())) {
                        return MatchState::succeed;
                    }
                    auto tok = manager.make_tree(result.kind(), result.token());
                    if (result.kind() == KeyWord::literal_keyword || result.kind() == KeyWord::literal_symbol) {
                        tok->left = current;
                        current = std::move(tok);
                    }
                    else {
                        if (!current) {
                            current = tok;
                        }
                        else if (current->left && !current->right) {
                            current->right = tok;
                        }
                        else {
                            result.error("invalid tree");
                            return MatchState::fatal;
                        }
                    }
                    return MatchState::succeed;
                }
            };

        }  // namespace tree

    }  // namespace syntax
}  // namespace utils
