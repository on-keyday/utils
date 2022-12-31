/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_if_for_switch - if, for and switch statement/expression
#pragma once
#include "minlpass.h"
#include "minl_block.h"

namespace utils {
    namespace minilang {
        namespace parser {
            constexpr auto for_if_stat_base(auto stat_name, bool none_expr_ok, int max_index, auto make_obj) {
                return [=](auto&& p) {
                    MINL_FUNC_LOG(stat_name)
                    MINL_BEGIN_AND_START(p.seq);
                    std::shared_ptr<MinNode> first, second, third;
                    int index = 0;
                    using ret_type = decltype(make_obj(index, first, second, third, block(call_stat())(p), Pos{start, p.seq.rptr}));
                    if (!expect_ident(p.seq, stat_name)) {
                        p.seq.rptr = begin;
                        return ret_type(nullptr);
                    }
                    auto space = [&] {
                        space::consume_space(p.seq, true);
                    };
                    auto error = [&](auto... msg) {
                        p.errc.say(msg...);
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return ret_type(nullptr);
                    };
                    auto expr = [&] {
                        auto old = p.loc;
                        p.loc = pass_loc::condition;
                        auto tmp = p.expr(p);
                        p.loc = old;
                        return tmp;
                    };
                    space();
                    if (none_expr_ok && p.seq.match("{")) {
                        goto END_OF_STAT;
                    }
                    index = 1;
                    if (!none_expr_ok || !p.seq.match(";")) {
                        first = expr();
                        if (!first) {
                            return error("expect ", stat_name, " statement condition or initialization but not");
                        }
                        space();
                        if (index == max_index || p.seq.match("{")) {
                            goto END_OF_STAT;
                        }
                    }
                    if (!seq.seek_if(";")) {
                        return error("expect ", stat_name, " statement ; but not");
                    }
                    space();
                    index = 2;
                    if (!none_expr_ok || !p.seq.match(";")) {
                        second = expr();
                        if (!second) {
                            return error("expect ", stat_name, " statement condition but not");
                        }
                        space();
                        if (index == max_index || p.seq.match("{")) {
                            goto END_OF_STAT;
                        }
                    }
                    if (!seq.seek_if(";")) {
                        return error("expect ", stat_name, " statement ; but not");
                    }
                    space();
                    index = 3;
                    if (!none_expr_ok || !p.seq.match("{")) {
                        third = expr();
                        if (!third) {
                            return error("expect ", stat_name, " statement expr per loop but not");
                        }
                        space();
                    }
                END_OF_STAT:
                    if (!p.seq.match("{")) {
                        return error("expect ", stat_name, " statement block begin { but not");
                    }
                    p.err = false;
                    auto node = block(call_stat())(p);
                    if (!node) {
                        return error("expect ", stat_name, " statement block but not");
                    }
                    return make_obj(index, first, second, third, node, Pos{start, p.seq.rptr});
                };
            }

            constexpr auto for_() {
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

            constexpr auto if_() {
                return [](auto&& p) -> std::shared_ptr<IfStatNode> {
                    constexpr auto a_stat = for_if_stat_base("if", false, 2, [](int index, auto&& first, auto&& second, auto&& third, auto&& block, Pos pos) {
                        auto root = std::make_shared<IfStatNode>();
                        root->str = if_stat_str_[index];
                        root->left = std::move(first);
                        root->right = std::move(second);
                        root->block = std::move(block);
                        root->pos = pos;
                        return root;
                    });
                    p.err = false;
                    std::shared_ptr<IfStatNode> if_ = a_stat(p);
                    if (!if_) {
                        return nullptr;
                    }
                    auto cur = if_;
                    while (true) {
                        const auto start = seq.rptr;
                        space::consume_space(p.seq, false);
                        if (!expect_ident(p.seq, "else")) {
                            p.seq.rptr = start;
                            return if_;
                        }
                        space::consume_space(p.seq, true);
                        if (seq.match("{")) {
                            auto els = block(call_stat())(p);
                            if (!els) {
                                errc.say("expect else statement but not");
                                errc.trace(start, p.seq);
                                p.err = true;
                                return nullptr;
                            }
                            cur->els = std::move(els);
                            return if_;
                        }
                        p.err = false;
                        auto elif_ = a_stat(p);
                        if (!elif_) {
                            errc.say("expect else if statement but not");
                            errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        cur->els = elif_;
                        cur = elif_;
                    }
                };
            }

            constexpr auto switch_() {
                return [](auto&& p) -> std::shared_ptr<SwitchStatNode> {
                    MINL_FUNC_LOG("switch")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, "switch")) {
                        seq.rptr = begin;
                        return nullptr;
                    }
                    space::consume_space(p.seq, true);
                    std::shared_ptr<MinNode> swcond;
                    if (!p.seq.match("{")) {
                        auto old = p.loc;
                        p.loc = pass_loc::condition;
                        swcond = p.expr(p);
                        p.loc = old;
                        if (!swcond) {
                            p.errc.say("expect switch statement condition expr but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                    }
                    space::consume_space(p.seq, true);
                    if (!p.seq.seek_if("{")) {
                        p.errc.say("expect switch statement begin { but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    auto root_switch = std::make_shared<SwitchStatNode>();
                    auto swnode = root_switch;
                    root_switch->str = switch_str_;
                    root_switch->expr = std::move(swcond);
                    std::shared_ptr<BlockNode> block = swnode;
                    while (true) {
                        space::consume_space(p.seq, true);
                        if (seq.eos()) {
                            p.errc.say("unexpected eof at switch statement");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        if (p.seq.seek_if("}")) {
                            break;
                        }
                        const auto stat_start = seq.rptr;
                        if (expect_ident(p.seq, "case")) {
                            auto old = p.loc;
                            p.loc = pass_loc::swtich_expr;
                            auto cas = p.expr(p);
                            p.loc = old;
                            if (!cas) {
                                errc.say("expect switch case condtion expr but not");
                                errc.trace(start, p.seq);
                                p.err = true;
                                return nullptr;
                            }
                            space::consume_space(p.seq, true);
                            if (!p.seq.seek_if(":")) {
                                p.errc.say("expect switch case : but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return nullptr;
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
                        if (expect_ident(p.seq, "default")) {
                            space::consume_space(p.seq, true);
                            if (!seq.seek_if(":")) {
                                p.errc.say("expect switch default : but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return nullptr;
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
                        p.err = false;
                        auto tmp = p.stat(p);
                        if (!tmp) {
                            p.errc.say("expect switch element statement but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        auto elm = std::make_shared<BlockNode>();
                        elm->str = block_element_str_;
                        elm->pos = {start, seq.rptr};
                        elm->element = std::move(tmp);
                        block->next = elm;
                        block = elm;
                    }
                    root_switch->pos = {start, seq.rptr};
                    return root_switch;
                };
            }

            constexpr auto match_() {
                return [](auto&& p) {
                    MINL_FUNC_LOG("match")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, "match")) {
                        seq.rptr = begin;
                        return nullptr;
                    }
                    space::consume_space(p.seq, true);
                    std::shared_ptr<MinNode> swcond;
                    auto old = p.loc;
                    p.loc = pass_loc::condition;
                    swcond = p.expr(p);
                    p.loc = old;
                    if (!swcond) {
                        p.errc.say("expect match statement condition expr but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    p.errc.say("unimplemented");
                    p.err = true;
                    return nullptr;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
