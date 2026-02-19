/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "branch_table.h"

namespace futils {
    namespace comb2::tree::node {

        template <class Tag = const char*>
        struct GenericNode {
            const bool is_group;
            Pos pos;
            Tag tag{};
            GenericNode(bool g)
                : is_group(g) {}
#ifdef _DEBUG
            virtual ~GenericNode() {}
#endif
        };

        template <class Tag = const char*>
        struct GenericGroup : GenericNode<Tag> {
            GenericGroup()
                : GenericNode<Tag>(true) {}
            std::weak_ptr<GenericGroup> parent;
            std::vector<std::shared_ptr<GenericNode<Tag>>> children;
        };

        template <class Tag = const char*>
        struct GenericToken : GenericNode<Tag> {
            GenericToken()
                : GenericNode<Tag>(false) {}
            std::string token;
        };

        template <class Tag = const char*>
        auto as_tok(auto&& tok) {
            auto v = static_cast<GenericToken<Tag>*>(std::to_address(tok));
            return v && !v->is_group ? v : nullptr;
        }

        template <class Tag = const char*>
        auto as_group(auto&& tok) {
            auto v = static_cast<GenericGroup<Tag>*>(std::to_address(tok));
            return v && v->is_group ? v : nullptr;
        }

        template <class Tag = const char*>
        inline auto collect(const std::shared_ptr<tree::Element>& elm) {
            std::shared_ptr<GenericGroup<Tag>> root = std::make_shared<GenericGroup<Tag>>();
            std::shared_ptr<GenericGroup<Tag>> current = root;
            auto cb = [&](auto& v, bool entry) {
                if constexpr (std::is_same_v<decltype(v), tree::Ident<Tag>&>) {
                    auto id = std::make_shared<GenericToken<Tag>>();
                    id->tag = v.tag;
                    id->token = v.ident;
                    id->pos = v.pos;
                    current->children.push_back(std::move(id));
                }
                else {
                    if (entry) {
                        auto child = std::make_shared<GenericGroup<Tag>>();
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
            tree::visit_nodes<Tag, Tag>(elm, cb);
            return root;
        }

        using Token = GenericToken<>;
        using Group = GenericGroup<>;
        using Node = GenericNode<>;

    }  // namespace comb2::tree::node
}  // namespace futils
