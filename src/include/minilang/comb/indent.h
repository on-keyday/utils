/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "nullctx.h"
#include "space.h"

namespace utils::minilang::comb::known {

    enum class IndentMode {
        current,
        new_,
        down,
        less,
        lesseq,
    };

    constexpr auto make_indent(auto&& count_fn, IndentMode mode) {
        return [=](auto&& seq, auto&& ctx, auto&&) {
            const size_t begin = seq.rptr;
            size_t indent = ctx.get_indent();
            size_t cur = 0;
            while (!seq.eos()) {
                auto c = count_fn(seq.current());
                if (c == 0) {
                    break;
                }
                cur += c;
                seq.consume();
            }
            if (mode == IndentMode::new_) {
                if (cur <= indent) {
                    seq.rptr = begin;
                    return CombStatus::not_match;
                }
                ctx.push_indent(cur);
            }
            else if (mode == IndentMode::current) {
                if (cur < indent) {
                    seq.rptr = begin;
                    return CombStatus::not_match;
                }
            }
            else if (mode == IndentMode::less) {
                if (cur >= indent) {
                    seq.rptr = begin;
                    return CombStatus::not_match;
                }
            }
            else if (mode == IndentMode::lesseq) {
                if (cur > indent) {
                    seq.rptr = begin;
                    return CombStatus::not_match;
                }
            }
            else {  // down
                if (cur >= indent) {
                    seq.rptr = begin;
                    return CombStatus::not_match;
                }
                ctx.pop_indent();
                indent = ctx.get_indent();
                seq.rptr = begin;
                cur = 0;
                while (cur < indent) {
                    auto c = count_fn(seq.current());
                    if (c == 0) {
                        break;
                    }
                    cur += c;
                    seq.consume();
                }
            }
            return CombStatus::match;
        };
    }

    constexpr auto judge_space = [](auto c) { return c == ' ' ? 1 : 0; };
    constexpr auto new_indent = make_indent(judge_space, IndentMode::new_);
    constexpr auto indent = make_indent(judge_space, IndentMode::current);
    constexpr auto down_indent = make_indent(judge_space, IndentMode::down);
    constexpr auto less_indent = make_indent(judge_space, IndentMode::less);
    constexpr auto lesseq_indent = make_indent(judge_space, IndentMode::lesseq);

    namespace test {
        constexpr bool check_indent() {
            struct L : NullCtx {
                size_t ind[10]{0};
                size_t idx = 0;
                constexpr size_t get_indent() {
                    return ind[idx];
                }

                constexpr void push_indent(size_t p) {
                    idx++;
                    ind[idx] = p;
                }

                constexpr void pop_indent() {
                    idx--;
                }
            } lctx;
            auto pre_indent = (bos | consume_line);
            auto ind = pre_indent & indent;
            auto new_ind = pre_indent & new_indent;
            auto down_ind = pre_indent & down_indent;
            auto seq = make_ref_seq("    \n    \n        \n    \n");
            auto parse = new_ind & ind & new_ind & down_ind & down_ind & eos;
            return parse(seq, lctx, 0) == CombStatus::match;
        }

        static_assert(check_indent());
    }  // namespace test
}  // namespace utils::minilang::comb::known
