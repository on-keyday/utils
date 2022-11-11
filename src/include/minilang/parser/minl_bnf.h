/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_bnf - bnf parser
#pragma once
#include "minlpass.h"
#include "minl_primitive.h"

namespace utils {
    namespace minilang {
        namespace parser {
            template <class Node = NumberNode>
            constexpr auto adjacent_integer(auto&& p, auto start, int radix, auto msg) {
                auto riread = radix_integer<Node>(radix, msg);
                return [=, &p]() -> std::shared_ptr<Node> {
                    if (!number::is_digit(p.seq.current())) {
                        p.errc.say(msg);
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    return riread(p);
                };
            }

            constexpr auto bnf_charcode() {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG("buf_code")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!p.seq.seek_if("%")) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    int radix = 0;
                    bool is_utf = false;
                    if (p.seq.seek_if("x") || p.seq.seek_if("X")) {
                        radix = 16;
                    }
                    else if (p.seq.seek_if("d") || p.seq.seek_if("D")) {
                        radix = 10;
                    }
                    else if (p.seq.seek_if("b") || p.seq.seek_if("B")) {
                        radix = 2;
                    }
                    else if (p.seq.seek_if("u") || p.seq.seek_if("U")) {
                        radix = 16;
                        is_utf = true;
                    }
                    else {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    auto read = adjacent_integer<CharNode>(p, start, radix, "expect BNF char code but not");
                    auto n1 = read();
                    if (!n1) {
                        return nullptr;
                    }
                    n1->is_utf = is_utf;
                    const auto range_s = p.seq.rptr;
                    if (p.seq.seek_if("-")) {
                        auto n2 = read();
                        if (!n2) {
                            return nullptr;
                        }
                        n2->is_utf = is_utf;
                        auto range = std::make_shared<RangeNode>();
                        range->str = "-";
                        range->pos = {range_s, range_s + 1};
                        range->min_ = std::move(n1);
                        range->max_ = std::move(n2);
                        return range;
                    }
                    return n1;
                };
            }

            enum class RangePrefixMode {
                disallow,
                normal_number,
                allow,
            };

            constexpr auto bnf_range_prefix(auto inner, RangePrefixMode mode = RangePrefixMode::disallow) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG("buf_range_prefix");
                    MINL_BEGIN_AND_START(p.seq);
                    const auto read = adjacent_integer(p, start, 10, "expect bnf range prefix but not");
                    std::shared_ptr<NumberNode> n1, n2;
                    if (number::is_digit(p.seq.current())) {
                        n1 = read();
                        if (!n1) {
                            return nullptr;
                        }
                    }
                    const auto r_start = p.seq.rptr;
                    auto inner_and_range = [&](auto r_start, auto r_end, auto min_, auto max_) -> std::shared_ptr<BinaryNode> {
                        auto right = inner(p);
                        if (!right) {
                            p.errc.say("expect right hand of bnf range prefix ~ but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        auto r = std::make_shared<RangeNode>();
                        r->str = "~";
                        r->min_ = std::move(min_);
                        r->max_ = std::move(max_);
                        r->pos = {r_start, r_end};
                        auto bin = std::make_shared<BinaryNode>();
                        bin->str = "~";
                        bin->left = std::move(r);
                        bin->right = std::move(right);
                        bin->pos = {r_start, r_end};
                        return bin;
                    };
                    if (!p.seq.seek_if("~")) {
                        if (n1 && mode != RangePrefixMode::normal_number) {
                            if (mode == RangePrefixMode::disallow) {
                                p.errc.say("expect bnf range prefix ~ but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return nullptr;
                            }
                            const auto r_end = p.seq.rptr;
                            return inner_and_range(start, r_end, n1, n1);
                        }
                        p.seq.rptr = begin;
                        return inner(p);
                    }
                    const auto r_end = p.seq.rptr;
                    if (number::is_digit(p.seq.current())) {
                        n2 = read();
                        if (!n2) {
                            return nullptr;
                        }
                    }
                    return inner_and_range(r_start, r_end, std::move(n1), std::move(n2));
                };
            }

        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
