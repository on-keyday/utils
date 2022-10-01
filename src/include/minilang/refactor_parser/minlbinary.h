/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "minlpass.h"

namespace utils {
    namespace minilang {
        namespace parser {
            struct Op {
                const char* op = nullptr;
                char errs[2]{};
            };

            constexpr auto expecter(auto... op) {
                return [=](auto& seq, auto& expected, Pos& pos) {
                    auto fold = [&](auto op) {
                        const size_t begin = seq.rptr;
                        helper::space::consume_space(seq, false);
                        const size_t start = seq.rptr;
                        if (seq.seek_if(op.op)) {
                            for (auto c : op.errs) {
                                if (c == 0) {
                                    break;
                                }
                                if (seq.current() == c) {
                                    seq.rptr = start;
                                    return false;
                                }
                            }
                            expected = op.op;
                            pos = {start, seq.rptr};
                            return true;
                        }
                        seq.rptr = begin;
                        return false;
                    };
                    return (... || fold(op));
                };
            }

            auto call_prim() {
                return [](auto&& p) {
                    return p.prim(p);
                };
            }

            auto binary(auto next, auto expect) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG("binary")
                    std::shared_ptr<MinNode> node = next(p);
                    if (!node) {
                        return nullptr;
                    }
                    Pos pos{};
                    const char* expected = nullptr;
                    while (expect(p.seq, expected, pos)) {
                        std::shared_ptr<MinNode> rhs = next(p);
                        if (!rhs) {
                            p.errc.say("expect right hand of operator ", expected, " but not");
                            p.errc.trace(pos.begin, p.seq);
                            return nullptr;
                        }
                        auto bin = std::make_shared<BinaryNode>();
                        bin->left = std::move(node);
                        bin->right = std::move(rhs);
                        bin->str = expected;
                        bin->pos = pos;
                        node = std::move(bin);
                    }
                    return node;
                };
            }

            auto assign(auto next, auto expect) {
                auto f = [=](auto& f, auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG("assign")
                    std::shared_ptr<MinNode> node = next(p);
                    if (!node) {
                        return nullptr;
                    }
                    Pos pos{};
                    const char* expected = nullptr;
                    while (expect(p.seq, expected, pos)) {
                        std::shared_ptr<MinNode> rhs = f(f, p);
                        if (!rhs) {
                            p.errc.say("expect right hand of operator ", expected, " but not");
                            p.errc.trace(pos.begin, p.seq);
                            return nullptr;
                        }
                        auto bin = std::make_shared<BinaryNode>();
                        bin->left = std::move(node);
                        bin->right = std::move(rhs);
                        bin->str = expected;
                        bin->pos = pos;
                        node = std::move(bin);
                    }
                    return node;
                };
                return [f](auto&& p) -> std::shared_ptr<MinNode> {
                    return f(f, p);
                };
            }

            struct Br {
                const char* op = nullptr;
                const char* begin = nullptr;
                const char* end = nullptr;
                const char* rem_comma = nullptr;
                bool suppression_on_cond = false;
            };

            auto parenthesis(auto... op) {
                return [](auto&& p) -> std::shared_ptr<BinaryNode> {
                    MINL_FUNC_LOG("parentesis");
                    std::shared_ptr<BinaryNode> node;
                    auto fold = [&](auto& op) {
                        MINL_FUNC_LOG()
                        MINL_BEGIN_AND_START(p.seq);
                        if (!p.seq.seek_if(op.begin)) {
                            p.seq.rptr = begin;
                            return false;
                        }
                        auto inner = p.expr(p);
                        if (!inner) {
                            p.errc.say("expect ", op.op, " inner element but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        helper::space::consume_space(p.seq, true);
                        if (!p.seq.seek_if(op.end)) {
                            p.errc.say("expect ", op.op, " end ", op.end, " but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        auto brac = std::make_shared<BinaryNode>();
                        brac->left = std::move(inner);
                        if (p.tmppass) {
                            brac->right = std::move(p.tmppass);
                        }
                        brac->pos = {start, p.seq.rptr};
                        node = std::move(brac);
                    };
                    (... || fold(op));
                    return node;
                };
            }

            auto dot(auto expecter) {
                return [=](auto&& p) -> std::shared_ptr<BinaryNode> {
                    MINL_FUNC_LOG("dot");
                    MINL_BEGIN_AND_START(p.seq);
                    const char* expected = nullptr;
                    Pos pos;
                    if (!expecter(p.seq, expected, pos)) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    auto old = p.loc;
                    p.loc = pass_loc::dot;
                    auto inner = p.prim(p);
                    p.loc = old;
                    if (!inner) {
                        p.errc.say("expect ", expected, " right primitive but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    auto brac = std::make_shared<BinaryNode>();
                    brac->left = std::move(inner);
                    if (p.tmppass) {
                        brac->right = std::move(p.tmppass);
                    }
                    brac->pos = pos;
                    return brac;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
