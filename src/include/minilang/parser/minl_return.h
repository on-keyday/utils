/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_return - return statement and other `keyword expr` style
#pragma once
#include "minlpass.h"
#include "minl_block.h"
#include "../../helper/readutil.h"

namespace utils {
    namespace minilang {
        namespace parser {
            constexpr auto break_continue_(auto word, auto ident, bool symbol = false) {
                return [=](auto&& p) -> std::shared_ptr<WordStatNode> {
                    MINL_FUNC_LOG(word)
                    MINL_BEGIN_AND_START(p.seq);
                    if (symbol) {
                        if (!p.seq.seek_if(word)) {
                            p.seq.rptr = begin;
                            return nullptr;
                        }
                    }
                    else {
                        if (!expect_ident(p.seq, word)) {
                            p.seq.rptr = begin;
                            return nullptr;
                        }
                    }
                    auto node = std::make_shared<WordStatNode>();
                    node->str = ident;
                    node->pos = {start, seq.rptr};
                    return node;
                };
            }

            constexpr auto extern_(auto word, auto ident, auto&& parse_after, bool not_must) {
                return [=](auto&& p) -> std::shared_ptr<WordStatNode> {
                    MINL_FUNC_LOG(word)
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, word)) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    std::shared_ptr<MinNode> after, block;
                    if (not_must) {
                        helper::space::consume_space(p.seq, true);
                        if (seq.match("{")) {
                            goto BLOCK;
                        }
                    }
                    p.after = parse_after(p);
                    if (!after) {
                        p.errc.say("expect ", word, " statement after but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                BLOCK:
                    helper::space::consume_space(p.seq, true);
                    if (!p.seq.match("{")) {
                        errc.say("expect ", word, " statement block begin { but not");
                        errc.trace(start, p.seq);
                        return nullptr;
                    }
                    p.err = false;
                    block = parser::block(call_stat())(p);
                    if (!block) {
                        p.errc.say("expect ", word, " statement block but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    auto tmp = std::make_shared<WordStatNode>();
                    tmp->str = ident;
                    tmp->pos = {start, seq.rptr};
                    tmp->expr = std::move(after);
                    tmp->block = std::move(block);
                    return tmp;
                };
            }

            constexpr auto return_(auto word, auto ident, auto&& parse_after, bool not_must) {
                return [=](auto&& p) -> std::shared_ptr<WordStatNode> {
                    MINL_FUNC_LOG(word)
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, word)) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    std::shared_ptr<MinNode> after;
                    if (not_must) {
                        auto tmp = p.seq.rptr;
                        helper::space::consume_space(p.seq, false);
                        if (helper::match_eol<false>(p.seq)) {
                            p.seq.rptr = tmp;
                            goto END;
                        }
                    }
                    p.err = false;
                    after = parse_after(p);
                    if (!after) {
                        p.errc.say("expect ", word, " statement after but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                END:
                    auto tmp = std::make_shared<WordStatNode>();
                    tmp->str = ident;
                    tmp->pos = {start, p.seq.rptr};
                    tmp->expr = std::move(after);
                    return tmp;
                };
            }

        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
