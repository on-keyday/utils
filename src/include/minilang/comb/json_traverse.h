/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "nullctx.h"
#include <string>

namespace utils::minilang::comb::branch {
    void skip_space(auto& it, const auto& node) {
        while (it != node.aend() && it->at("spaces")) {
            it++;
        }
    }

    template <class Node>
    const Node* expect(auto& it, const Node& node, const char* s) {
        if (it == node.aend()) {
            return nullptr;
        }
        auto ret = it->at(s);
        if (ret) {
            it++;
        }
        return ret;
    }

    template <class Node>
    const Node* expect_with_pos(auto& it, const Node& node, const char* s, Pos* pos) {
        if (it == node.aend()) {
            return nullptr;
        }
        auto ret = it->at(s);
        if (ret) {
            if (pos) {
                auto p = it->at("pos");
                if (!p) {
                    return nullptr;
                }
                const Node* b = p->at("begin");
                const Node* e = p->at("end");
                if (!b || !e ||
                    !b->as_number(pos->begin) ||
                    !e->as_number(pos->end)) {
                    return nullptr;
                }
            }
            it++;
        }
        return ret;
    }

    template <class Node>
    const Node* expect_token(auto& it, const Node& node, const char* tok = nullptr, Pos* pos = nullptr) {
        auto exp = expect_with_pos(it, node, "token", pos);
        if (exp && tok) {
            if (exp->template force_as_string<std::string>() != tok) {
                it--;
                return nullptr;
            }
        }
        return exp;
    }

}  // namespace utils::minilang::comb::branch
