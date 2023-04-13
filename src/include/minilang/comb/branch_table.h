/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>
#include "nullctx.h"
#include "../../helper/defer.h"

namespace utils::minilang::comb::branch {
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
    };

    struct Branch : Element {
        Branch()
            : Element(ElmType::branch) {}
        CombMode mode = CombMode::seq;
        CombStatus status = CombStatus::not_match;
        std::uint64_t id = 0;
        std::weak_ptr<Branch> parent;
        std::vector<std::shared_ptr<Element>> child;

       protected:
        Branch(ElmType t)
            : Element(t) {}
    };

    struct Token : Element {
        Token()
            : Element(ElmType::token) {}
        const char* token = nullptr;
        Pos pos;
    };

    template <class Tag>
    struct Group : Branch {
        Tag tag;
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

    inline std::shared_ptr<Token> is_Token(const std::shared_ptr<Element>& elm) {
        if (elm && elm->type == ElmType::token) {
            return std::static_pointer_cast<Token>(elm);
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
        bool discard_not_match = false;

        void group(auto tag, std::uint64_t id) {
            auto br = std::make_shared<Group<decltype(tag)>>();
            br->id = id;
            br->tag = tag;
            if (!root_branch) {
                root_branch = br;
            }
            else {
                current_branch->child.push_back(br);
            }
            br->parent = current_branch;
            br->mode = CombMode::group;
            current_branch = br;
        }

        void group_end(auto&& tag, std::uint64_t id, CombState s) {
            assert(current_branch->id == id);
            auto br = current_branch->parent.lock();
            current_branch->status = s == CombState::match ? CombStatus::match : CombStatus::not_match;
            current_branch = br;
        }

        void branch(CombMode mode, std::uint64_t id) {
            auto br = std::make_shared<Branch>();
            br->id = id;
            if (!root_branch) {
                root_branch = br;
            }
            else {
                current_branch->child.push_back(br);
            }
            br->parent = current_branch;
            br->mode = mode;
            current_branch = br;
        }

       private:
        void do_discard(CombStatus s) {
            if (discard_not_match && s == CombStatus::not_match) {
                current_branch->child.pop_back();  // discard previous
            }
        }

       public:
        void commit(CombMode m, std::uint64_t id, CombState s) {
            assert(current_branch->id == id);
            auto br = current_branch->parent.lock();
            const auto check = s == CombState::match
                                   ? CombStatus::match
                               : s == CombState::fatal
                                   ? CombStatus::fatal
                                   : CombStatus::not_match;
            if (m == CombMode::branch) {
                current_branch->status = check;
                current_branch = br;
                do_discard(check);
                if (s == CombState::other_branch) {
                    branch(m, id);
                }
            }
            else if (m == CombMode::repeat) {
                current_branch->status = check;
                current_branch = br;
                do_discard(check);
                if (s == CombState::match) {
                    branch(m, id);
                }
            }
            else if (m == CombMode::optional || m == CombMode::must_match) {
                current_branch->status = check;
                current_branch = br;
                do_discard(check);
            }
        }

        void ctoken(const char* c, Pos pos) {
            auto tok = std::make_shared<Token>();
            tok->token = c;
            tok->pos = pos;
            current_branch->child.push_back(tok);
        }

        void crange(auto tag, const std::string& str, Pos pos) {
            auto tok = std::make_shared<Ident<decltype(tag)>>();
            tok->tag = tag;
            tok->ident = str;
            tok->pos = pos;
            current_branch->child.push_back(tok);
        }

       private:
        static CallNext iterate_each(const std::shared_ptr<Branch>& r, auto&& call) {
            CallNext cs = call(r, CallEntry::enter);
            if (cs == CallNext::skip || cs == CallNext::fin) {
                return cs;
            }
            const auto f = helper::defer([&] { call(r, CallEntry::leave); });
            for (auto& v : r->child) {
                auto b = is_Branch(v);
                if (b) {
                    cs = iterate_each(b, call);
                }
                else {
                    cs = call(v, CallEntry::token);
                }
                if (cs == CallNext::fin) {
                    return cs;
                }
            }
            return CallNext::cont;
        }

       public:
        void iterate(auto&& call) const {
            if (!root_branch) {
                return;
            }
            iterate_each(root_branch, call);
        }
    };

}  // namespace utils::minilang::comb::branch
