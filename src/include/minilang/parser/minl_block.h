/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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

            constexpr auto or_(auto... op) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG("or_op");
                    std::shared_ptr<MinNode> node;
                    auto fold = [&](auto& op) {
                        p.err = false;
                        node = op(p);
                        if (!node) {
                            if (p.err) {
                                return true;
                            }
                            return false;
                        }
                        return true;
                    };
                    (... || fold(op));
                    return node;
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
                        helper::space::consume_space(p.seq, true);
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
                        cur = std::move(new_cur);
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
                    block->str = block_statement_str_;
                    auto cur = block;
                    while (true) {
                        const auto tmp_begin = p.seq.rptr;
                        helper::space::consume_space(p.seq, true);
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
                        new_cur->str = block_element_str_;
                        new_cur->pos = {tmp_start, p.seq.rptr};
                        new_cur->element = std::move(node);
                        cur = std::move(new_cur);
                    }
                    block->pos = {start, p.seq.rptr};
                    return block;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
