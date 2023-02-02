/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_block - block
#pragma once
#include "minlpass.h"

namespace utils {
    namespace minilang {
        namespace parser {

            auto call_stat() {
                return [](auto&& p) {
                    return p.stat(p);
                };
            }

            auto block(auto inner) {
                return [=](auto&& p) -> std::shared_ptr<BlockNode> {
                    MINL_FUNC_LOG("block");
                    MINL_BEGIN_AND_START(p.seq);
                    if (!p.seq.seek_if("{")) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    auto block = std::make_shared<BlockNode>();
                    block->str = block_statement_str_;
                    auto cur = block;
                    while (true) {
                        const auto tmp_begin = p.seq.rptr;
                        space::consume_space(p.seq, true);
                        const auto tmp_start = p.seq.rptr;
                        if (p.seq.eos()) {
                            p.errc.say("unexpected eof at block statement. expect }.");
                            p.errc.trace(start, p.seq.rptr);
                            p.err = true;
                            return nullptr;
                        }
                        if (p.seq.seek_if("}")) {
                            break;
                        }
                        auto node = inner(p);
                        if (!node) {
                            p.errc.say("expect block statement element but not");
                            p.errc.trace(start, p.seq.rptr);
                            p.err = true;
                            return nullptr;
                        }
                        auto new_cur = std::make_shared<BlockNode>();
                        new_cur->str = block_element_str_;
                        new_cur->pos = {tmp_start, p.seq.rptr};
                        new_cur->element = std::move(node);
                        cur->next = new_cur;
                        cur = std::move(new_cur);
                    }
                    block->pos = {start, p.seq.rptr};
                    return block;
                };
            }

            auto eol_factor(auto inner) {
                return [=](auto&& p) -> decltype(inner(p)) {
                    const auto start = p.seq.rptr;
                    space::consume_space(p.seq, false);
                    if (space::parse_eol<true>(p.seq)) {
                        return nullptr;
                    }
                    return inner(p);
                };
            }

            auto repeat(bool least_one, bool consume_line, auto inner) {
                return [=](auto&& p) -> std::shared_ptr<BlockNode> {
                    MINL_FUNC_LOG("repeat");
                    const auto start = p.seq.rptr;
                    auto block = std::make_shared<BlockNode>();
                    block->str = repeat_str_;
                    auto cur = block;
                    bool one = false;
                    while (true) {
                        const auto tmp_begin = p.seq.rptr;
                        space::consume_space(p.seq, consume_line);
                        const auto tmp_start = p.seq.rptr;
                        if (p.seq.eos()) {
                            break;
                        }
                        auto node = inner(p);
                        if (!node) {
                            if (p.err) {
                                return nullptr;
                            }
                            break;
                        }
                        one = true;
                        auto new_cur = std::make_shared<BlockNode>();
                        new_cur->str = repeat_element_str_;
                        new_cur->pos = {tmp_start, p.seq.rptr};
                        new_cur->element = std::move(node);
                        cur->next = new_cur;
                        cur = std::move(new_cur);
                    }
                    if (least_one && !one) {
                        p.errc.say("least one repeat element required but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    block->pos = {start, p.seq.rptr};
                    return block;
                };
            }

            auto program(auto inner) {
                return [=](auto&& p) {
                    MINL_FUNC_LOG("program");
                    const auto start = p.seq.rptr;
                    auto block = std::make_shared<BlockNode>();
                    block->str = program_str_;
                    auto cur = block;
                    while (true) {
                        const auto tmp_begin = p.seq.rptr;
                        space::consume_space(p.seq, true);
                        const auto tmp_start = p.seq.rptr;
                        if (p.seq.eos()) {
                            break;
                        }
                        auto node = inner(p);
                        if (!node) {
                            p.errc.say("expect block statement element but not");
                            p.errc.trace(start, p.seq.rptr);
                            p.err = true;
                            return nullptr;
                        }
                        auto new_cur = std::make_shared<BlockNode>();
                        new_cur->str = program_element_str_;
                        new_cur->pos = {tmp_start, p.seq.rptr};
                        new_cur->element = std::move(node);
                        cur->next = new_cur;
                        cur = std::move(new_cur);
                    }
                    block->pos = {start, p.seq.rptr};
                    return block;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
