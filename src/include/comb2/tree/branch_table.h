/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../status.h"
#include "../pos.h"
#include "../cbtype.h"
#include "../../helper/defer.h"
#include <cassert>
#include "../seqrange.h"
namespace futils::comb2::tree {
    enum class ElmType {
        branch,
        token,
        ident,
        group,
    };

    struct Element {
        const ElmType type;

       protected:
        Element(ElmType t)
            : type(t) {}
#ifdef _DEBUG
        virtual ~Element() {}
#endif
    };

    struct Branch : Element {
        Branch()
            : Element(ElmType::branch) {}

        std::weak_ptr<Branch> parent;
        std::vector<std::shared_ptr<Element>> child;

       protected:
        Branch(ElmType t)
            : Element(t) {}
    };

    template <class Tag>
    struct Group : Branch {
        Tag tag;
        Pos pos;
        Group()
            : Branch(ElmType::group) {}
    };

    template <class Tag>
    struct Ident : Element {
        Ident()
            : Element(ElmType::ident) {}
        Tag tag;
        std::string ident;
        Pos pos;
    };

    inline std::shared_ptr<Branch> is_Branch(const std::shared_ptr<Element>& elm) {
        if (elm && (elm->type == ElmType::branch ||
                    elm->type == ElmType::group)) {
            return std::static_pointer_cast<Branch>(elm);
        }
        return nullptr;
    }

    template <class Tag>
    inline std::shared_ptr<Group<Tag>> is_Group(const std::shared_ptr<Element>& elm) {
        if (elm && elm->type == ElmType::group) {
            return std::static_pointer_cast<Group<Tag>>(elm);
        }
        return nullptr;
    }

    template <class Tag>
    inline std::shared_ptr<Ident<Tag>> is_Ident(const std::shared_ptr<Element>& elm) {
        if (elm && elm->type == ElmType::ident) {
            return std::static_pointer_cast<Ident<Tag>>(elm);
        }
        return nullptr;
    }

    enum class CallNext {
        cont,
        skip,
        fin,
    };

    enum class CallEntry {
        enter,
        leave,
        token,
    };

    struct BranchTable {
        std::shared_ptr<Branch> root_branch;
        std::shared_ptr<Branch> current_branch;
        // if str_count > 0 then ignore branch making
        size_t str_count = 0;
        // if lexer_mode is true then conditional state holder is disabled
        // this feature is experimental
        bool lexer_mode = false;

        void maybe_init() {
            if (!root_branch) {
                root_branch = std::make_shared<Branch>();
                current_branch = root_branch;
            }
        }

        void begin_group(auto&& tag) {
            if (str_count) {
                return;
            }
            auto br = std::make_shared<Group<std::decay_t<decltype(tag)>>>();
            br->tag = tag;
            maybe_init();
            current_branch->child.push_back(br);
            br->parent = current_branch;
            current_branch = br;
        }

        void end_group(Status res, auto&& tag, Pos pos) {
            if (str_count) {
                return;
            }
            assert(current_branch->type == ElmType::group);
            static_cast<Group<std::decay_t<decltype(tag)>>*>(current_branch.get())->pos = pos;
            auto br = current_branch->parent.lock();
            current_branch = br;
            if (res == Status::not_match) {
                discard_child();
            }
        }

        void logic_entry(CallbackType entry) {
            if (entry == CallbackType::peek_begin) {
                str_count++;
                return;
            }
            if (str_count) {
                return;
            }
            if (lexer_mode) {
                return;
            }
            auto br = std::make_shared<Branch>();
            maybe_init();
            current_branch->child.push_back(br);
            br->parent = current_branch;
            current_branch = br;
        }

       private:
        void discard_child() {
            current_branch->child.pop_back();  // discard previous
        }

       public:
        void logic_result(CallbackType entry, Status status) {
            if (entry == CallbackType::peek_end) {
                str_count--;
                return;
            }
            if (str_count) {
                return;
            }
            if (lexer_mode) {
                return;
            }
            auto br = current_branch->parent.lock();
            current_branch = br;
            if (status == Status::not_match) {
                discard_child();
            }
        }

        void begin_string(auto&& tag) {
            str_count++;
        }

        void end_string(Status res, auto&& tag, const auto& seq, Pos pos) {
            str_count--;
            if (str_count) {
                return;
            }
            if (res == Status::not_match) {
                return;
            }
            auto tok = std::make_shared<Ident<std::decay_t<decltype(tag)>>>();
            tok->tag = tag;
            seq_range_to_string(tok->ident, seq, pos);
            tok->pos = pos;
            maybe_init();
            current_branch->child.push_back(tok);
        }
    };

    template <class IdentTag = const char*, class GroupTag = const char*>
    void visit_nodes(const std::shared_ptr<Element>& elm, auto&& cb) {
        if (auto id = comb2::tree::is_Ident<IdentTag>(elm)) {
            cb(*id, true);
        }
        else if (auto v = comb2::tree::is_Group<GroupTag>(elm)) {
            cb(*v, true);
            const auto d = futils::helper::defer([&] {
                cb(*v, false);
            });
            for (auto& e : v->child) {
                visit_nodes<IdentTag, GroupTag>(e, cb);
            }
        }
        else if (auto v = comb2::tree::is_Branch(elm)) {
            for (auto& e : v->child) {
                visit_nodes<IdentTag, GroupTag>(e, cb);
            }
        }
    }

    template <class IdentTag = const char*, class GroupTag = const char*>
    void visit_nodes_raw(const std::shared_ptr<Element>& elm, auto&& cb) {
        if (auto id = comb2::tree::is_Ident<IdentTag>(elm)) {
            cb(*id, true);
        }
        else if (auto v = comb2::tree::is_Group<GroupTag>(elm)) {
            cb(*v, true);
            const auto d = futils::helper::defer([&] {
                cb(*v, false);
            });
            for (auto& e : v->child) {
                visit_nodes_raw(e, cb);
            }
        }
        else if (auto v = comb2::tree::is_Branch(elm)) {
            cb(*v, true);
            const auto d = futils::helper::defer([&] {
                cb(*v, false);
            });
            for (auto& e : v->child) {
                visit_nodes_raw(e, cb);
            }
        }
    }

}  // namespace futils::comb2::tree
