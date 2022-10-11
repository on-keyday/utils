/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlctrl - minl control block
#pragma once
#include "minl.h"

namespace utils {
    namespace minilang {

        constexpr auto block_stat() {
            return [](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG("block_stat")
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!seq.seek_if("{")) {
                    return false;
                }
                auto root = std::make_shared<BlockNode>();
                root->str = block_statement_str_;
                auto cur = root;
                while (true) {
                    helper::space::consume_space(seq, true);
                    if (seq.seek_if("}")) {
                        break;
                    }
                    std::shared_ptr<MinNode> tmp;
                    err = false;
                    const auto elm_start = seq.rptr;
                    if (!stat(stat, expr, seq, tmp, err, errc) || err) {
                        errc.say("expect inner block statement but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto elm = std::make_shared<BlockNode>();
                    elm->element = std::move(tmp);
                    elm->str = block_element_str_;
                    elm->pos = {elm_start, seq.rptr};
                    cur->next = elm;
                    cur = elm;
                }
                root->pos = {start, seq.rptr};
                node = std::move(root);
                return true;
            };
        }

        constexpr auto for_if_stat_base(auto stat_name, bool none_expr_ok, int max_index, auto make_obj) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG(stat_name)
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!expect_ident(seq, stat_name)) {
                    return false;
                }
                auto space = [&] {
                    helper::space::consume_space(seq, true);
                };

                std::shared_ptr<MinNode> first, second, third;
                int index = 0;
                auto error = [&](auto... msg) {
                    errc.say(msg...);
                    errc.trace(start, seq);
                    err = true;
                    return true;
                };
                space();
                if (none_expr_ok && seq.match("{")) {
                    goto END_OF_STAT;
                }
                index = 1;
                if (!none_expr_ok || !seq.match(";")) {
                    first = expr(seq, brc_cond);
                    if (!first) {
                        return error("expect ", stat_name, " statement condition or initialization but not");
                    }
                    space();
                    if (index == max_index || seq.match("{")) {
                        goto END_OF_STAT;
                    }
                }
                if (!seq.seek_if(";")) {
                    return error("expect ", stat_name, " statement ; but not");
                }
                space();
                index = 2;
                if (!none_expr_ok || !seq.match(";")) {
                    second = expr(seq, brc_cond);
                    if (!second) {
                        return error("expect ", stat_name, " statement condition but not");
                    }
                    space();
                    if (index == max_index || seq.match("{")) {
                        goto END_OF_STAT;
                    }
                }
                if (!seq.seek_if(";")) {
                    return error("expect ", stat_name, " statement ; but not");
                }
                space();
                index = 3;
                if (!none_expr_ok || !seq.match("{")) {
                    third = expr(seq, brc_cond);
                    if (!third) {
                        return error("expect ", stat_name, " statement expr per loop but not");
                    }
                    space();
                }
            END_OF_STAT:
                if (!seq.match("{")) {
                    return error("expect ", stat_name, " statement block begin { but not");
                }
                std::shared_ptr<MinNode> block;
                err = false;
                if (!stat(stat, expr, seq, block, err, errc) || err) {
                    return error("expect ", stat_name, " statment block but not");
                }

                node = make_obj(index, first, second, third, block, Pos{start, seq.rptr});
                return true;
            };
        }

        constexpr auto for_statement() {
            return for_if_stat_base("for", true, 3, [](int index, auto&& first, auto&& second, auto&& third, auto&& block, Pos pos) {
                auto root = std::make_shared<ForStatNode>();
                root->str = for_stat_str_[index];
                root->left = std::move(first);
                root->center = std::move(second);
                root->right = std::move(third);
                root->block = std::move(block);
                root->pos = pos;
                return root;
            });
        }

        constexpr auto if_statement() {
            return [](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                constexpr auto a_stat = for_if_stat_base("if", false, 2, [](int index, auto&& first, auto&& second, auto&& third, auto&& block, Pos pos) {
                    auto root = std::make_shared<IfStatNode>();
                    root->str = if_stat_str_[index];
                    root->left = std::move(first);
                    root->right = std::move(second);
                    root->block = std::move(block);
                    root->pos = pos;
                    return root;
                });
                err = false;
                if (!a_stat(stat, expr, seq, node, err, errc) || err) {
                    return err;
                }
                auto if_ = std::static_pointer_cast<IfStatNode>(node);
                while (true) {
                    const auto start = seq.rptr;
                    helper::space::consume_space(seq, false);
                    if (!expect_ident(seq, "else")) {
                        seq.rptr = start;
                        return true;
                    }
                    helper::space::consume_space(seq, true);
                    if (seq.match("{")) {
                        std::shared_ptr<MinNode> tmp;
                        if (!stat(stat, expr, seq, tmp, err, errc)) {
                            errc.say("expect else block but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        if_->els = std::move(tmp);
                        return true;
                    }
                    std::shared_ptr<MinNode> tmp;
                    err = false;
                    if (!a_stat(stat, expr, seq, tmp, err, errc)) {
                        errc.say("expect else if statement but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto elif_ = std::static_pointer_cast<IfStatNode>(tmp);
                    if_->els = elif_;
                    if_ = elif_;
                }
            };
        }

        constexpr auto switch_statement() {
            return [](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                const auto start = seq.rptr;
                helper::space::consume_space(seq, true);
                if (!expect_ident(seq, "switch")) {
                    seq.rptr = start;
                    return false;
                }
                helper::space::consume_space(seq, true);
                std::shared_ptr<MinNode> swcond;
                if (!seq.match("{")) {
                    swcond = expr(seq, brc_cond);
                    if (!swcond) {
                        errc.say("expect switch statement condition expr but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                }
                helper::space::consume_space(seq, true);
                if (!seq.seek_if("{")) {
                    errc.say("expect switch statement begin { but not");
                    errc.trace(start, seq);
                    err = true;
                    return true;
                }
                auto root_switch = std::make_shared<SwitchStatNode>();
                auto swnode = root_switch;
                root_switch->str = switch_str_;
                root_switch->expr = std::move(swcond);
                std::shared_ptr<BlockNode> block = swnode;
                while (true) {
                    helper::space::consume_space(seq, true);
                    if (seq.eos()) {
                        errc.say("unexpected eof at switch statement");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    if (seq.seek_if("}")) {
                        break;
                    }
                    const auto stat_start = seq.rptr;
                    if (expect_ident(seq, "case")) {
                        auto cas = expr(seq);
                        if (!cas) {
                            errc.say("expect switch case condtion expr but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        helper::space::consume_space(seq, true);
                        if (!seq.seek_if(":")) {
                            errc.say("expect switch case : but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        auto cnode = std::make_shared<SwitchStatNode>();
                        cnode->str = case_str_;
                        cnode->pos = {stat_start, seq.rptr};
                        cnode->expr = cas;
                        swnode->next_case = cnode;
                        swnode = cnode;
                        block = cnode;
                        continue;
                    }
                    if (expect_ident(seq, "default")) {
                        helper::space::consume_space(seq, true);
                        if (!seq.seek_if(":")) {
                            errc.say("expect switch default : but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        auto cnode = std::make_shared<SwitchStatNode>();
                        cnode->str = default_str_;
                        cnode->pos = {stat_start, seq.rptr};
                        swnode->next_case = cnode;
                        swnode = cnode;
                        block = cnode;
                        continue;
                    }
                    std::shared_ptr<MinNode> tmp;
                    err = false;
                    if (!stat(stat, expr, seq, tmp, err, errc) || err) {
                        errc.say("expect switch element statement but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto elm = std::make_shared<BlockNode>();
                    elm->str = block_element_str_;
                    elm->pos = {start, seq.rptr};
                    elm->element = std::move(tmp);
                    block->next = elm;
                    block = elm;
                }
                root_switch->pos = {start, seq.rptr};
                node = std::move(root_switch);
                return true;
            };
        }

        constexpr auto an_expr() {
            return [](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG("an_expr")
                const auto start = seq.rptr;
                node = expr(seq);
                if (!node) {
                    err = true;
                    errc.say("expect expression but not");
                    errc.trace(start, seq);
                }
                return true;
            };
        }

        constexpr auto one_word(auto word, auto ident) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG(word)
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!expect_ident(seq, word)) {
                    return false;
                }
                node = std::make_shared<WordStatNode>();
                node->str = ident;
                node->pos = {start, seq.rptr};
                return true;
            };
        }

        constexpr auto one_word_symbol(auto word, auto ident) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG(word)
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!seq.seek_if(word)) {
                    return false;
                }
                node = std::make_shared<WordStatNode>();
                node->str = ident;
                node->pos = {start, seq.rptr};
                return true;
            };
        }

        constexpr auto one_word_plus_and_block(auto word, auto ident, auto&& parse_after, bool not_must) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG(word)
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!expect_ident(seq, word)) {
                    return false;
                }
                std::shared_ptr<MinNode> after, block;
                if (not_must) {
                    helper::space::consume_space(seq, true);
                    if (seq.match("{")) {
                        goto BLOCK;
                    }
                }
                after = parse_after(seq, errc);
                if (!after) {
                    errc.say("expect ", word, " statement after but not");
                    errc.trace(start, seq);
                    err = true;
                    return true;
                }
            BLOCK:
                helper::space::consume_space(seq, true);
                if (!seq.match("{")) {
                    errc.say("expect ", word, " statement block begin { but not");
                    errc.trace(start, seq);
                    return false;
                }
                err = false;
                if (!stat(stat, expr, seq, block, err, errc) || err) {
                    errc.say("expect ", word, " statement block but not");
                    errc.trace(start, seq);
                    return false;
                }
                auto tmp = std::make_shared<WordStatNode>();
                tmp->str = ident;
                tmp->pos = {start, seq.rptr};
                tmp->expr = std::move(after);
                tmp->block = std::move(block);
                node = std::move(tmp);
                return true;
            };
        }

        constexpr auto one_word_plus_expr(auto word, auto ident, bool not_must) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG(word)
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!expect_ident(seq, word)) {
                    return false;
                }
                std::shared_ptr<MinNode> after;
                if (not_must) {
                    helper::space::consume_space(seq, false);
                    if (helper::match_eol<false>(seq)) {
                        goto END;
                    }
                }
                after = expr(seq);
                if (!after) {
                    errc.say("expect ", word, " statement after expr but not");
                    errc.trace(start, seq);
                    err = true;
                    return true;
                }
            END:
                auto tmp = std::make_shared<WordStatNode>();
                tmp->str = ident;
                tmp->pos = {start, seq.rptr};
                tmp->expr = std::move(after);
                node = std::move(tmp);
                return true;
            };
        }

        constexpr auto one_word_plus(auto word, auto ident, auto&& parse_after) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG(word)
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!expect_ident(seq, word)) {
                    return false;
                }
                auto after = parse_after(seq, errc);
                if (!after) {
                    errc.say("expect ", word, " statement after but not");
                    errc.trace(start, seq);
                    return false;
                }
                auto tmp = std::make_shared<WordStatNode>();
                tmp->str = ident;
                tmp->pos = {start, seq.rptr};
                tmp->expr = std::move(after);
                node = std::move(tmp);
                return true;
            };
        }

    }  // namespace minilang
}  // namespace utils
