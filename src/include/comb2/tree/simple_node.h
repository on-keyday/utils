/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "branch_table.h"

namespace futils {
    namespace comb2::tree::node {

        struct Node {
            const bool is_group;
            Pos pos;
            const char *tag = nullptr;
            Node(bool g)
                : is_group(g) {}
#ifdef _DEBUG
            virtual ~Node() {}
#endif
        };

        struct Group : Node {
            Group()
                : Node(true) {}
            std::weak_ptr<Group> parent;
            std::vector<std::shared_ptr<Node>> children;
        };

        struct Token : Node {
            Token()
                : Node(false) {}
            std::string token;
        };

        auto as_tok(auto &&tok) {
            auto v = static_cast<Token *>(std::to_address(tok));
            return v && !v->is_group ? v : nullptr;
        }

        auto as_group(auto &&tok) {
            auto v = static_cast<Group *>(std::to_address(tok));
            return v && v->is_group ? v : nullptr;
        }

        // Ident or Group, which is derived from Element, must have const char* tag
        inline auto collect(const std::shared_ptr<tree::Element> &elm) {
            std::shared_ptr<Group> root = std::make_shared<Group>();
            std::shared_ptr<Group> current = root;
            auto cb = [&](auto &v, bool entry) {
                if constexpr (std::is_same_v<decltype(v), tree::Ident<const char *> &>) {
                    auto id = std::make_shared<Token>();
                    id->tag = v.tag;
                    id->token = v.ident;
                    id->pos = v.pos;
                    current->children.push_back(std::move(id));
                }
                else {
                    if (entry) {
                        auto child = std::make_shared<Group>();
                        child->parent = current;
                        child->tag = v.tag;
                        child->pos = v.pos;
                        current->children.push_back(child);
                        current = child;
                    }
                    else {
                        current = current->parent.lock();
                    }
                }
            };
            tree::visit_nodes(elm, cb);
            return root;
        }

    }  // namespace comb2::tree::node
}  // namespace futils
