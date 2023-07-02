/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

            constexpr auto expecter(bool consume_line, auto... op) {
                return [=](auto& seq, auto& expected, Pos& pos) {
                    auto fold = [&](auto op) {
                        const size_t begin = seq.rptr;
                        strutil::consume_space(seq, consume_line);
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

            constexpr auto call_prim() {
                return [](auto&& p) {
                    return p.prim(p);
                };
            }

            constexpr auto call_expr() {
                return [](auto&& p) {
                    return p.expr(p);
                };
            }

            constexpr auto unary(bool self_repeat, auto next, auto expect) {
                auto f = [=](auto&& f, auto&& p) -> std::shared_ptr<MinNode> {
                    const char* expected = 0;
                    Pos pos{};
                    if (expect(p.seq, expected, pos)) {
                        std::shared_ptr<MinNode> node;
                        if (self_repeat) {
                            node = f(f, p);
                        }
                        else {
                            node = next(p);
                        }
                        if (!node) {
                            return nullptr;
                        }
                        auto bin = std::make_shared<BinaryNode>();
                        bin->pos = pos;
                        bin->str = expected;
                        bin->right = std::move(node);
                        return bin;
                    }
                    return next(p);
                };
                return [f](auto&& p) {
                    return f(f, p);
                };
            }

            constexpr auto asymmetric(auto log, auto left, auto right, auto expect) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG(log)
                    auto node = left(p);
                    if (!node) {
                        return nullptr;
                    }
                    Pos pos{};
                    const char* expected = nullptr;
                    while (expect(p.seq, expected, pos)) {
                        std::shared_ptr<MinNode> rhs = right(p);
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

            constexpr auto binary(auto next, auto expect) {
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
                const char* sep = nullptr;
            };

            constexpr auto parenthesis(auto inner, auto... br) {
                return [=](auto&& p) -> std::shared_ptr<BinaryNode> {
                    MINL_FUNC_LOG("parentesis");
                    std::shared_ptr<BinaryNode> node;
                    auto fold = [&](auto& br) {
                        MINL_BEGIN_AND_START(p.seq);
                        if (!p.seq.seek_if(br.begin)) {
                            p.seq.rptr = begin;
                            return false;
                        }
                        auto in = inner(p);
                        if (!in) {
                            p.errc.say("expect ", br.op, " inner element but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        strutil::consume_space(p.seq, true);
                        if (!p.seq.seek_if(br.end)) {
                            p.errc.say("expect ", br.op, " end ", br.end, " but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        auto brac = std::make_shared<BinaryNode>();
                        brac->right = std::move(in);
                        if (p.tmppass) {
                            brac->left = std::move(p.tmppass);
                        }
                        brac->pos = {start, p.seq.rptr};
                        brac->str = br.op;
                        node = std::move(brac);
                        return true;
                    };
                    (void)(... || fold(br));
                    return node;
                };
            }

            constexpr auto unary_after(auto expecter) {
                return [=](auto&& p) -> std::shared_ptr<BinaryNode> {
                    MINL_FUNC_LOG("after_unary")
                    MINL_BEGIN_AND_START(p.seq);
                    const char* expected = nullptr;
                    Pos pos;
                    if (!expecter(p.seq, expected, pos)) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    auto bin = std::make_shared<BinaryNode>();
                    bin->str = expected;
                    bin->pos = pos;
                    bin->right = std::move(p.tmppass);
                    return bin;
                };
            }

            constexpr auto dot(auto expecter) {
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

            constexpr auto slices(auto... br) {
                return [=](auto&& p) {
                    MINL_FUNC_LOG("indexes");
                    std::shared_ptr<BinaryNode> node;
                    auto fold = [&](auto& br) -> bool {
                        MINL_BEGIN_AND_START(p.seq);
                        if (!p.seq.seek_if(br.begin)) {
                            p.seq.rptr = begin;
                            return false;
                        }
                        auto cur = p.expr(p);
                        if (!cur) {
                            p.errc.say("expect ", br.op, " inner element but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        while (true) {
                            strutil::consume_space(p.seq, true);
                            const auto sep_start = p.seq.rptr;
                            if (p.seq.seek_if(br.sep)) {
                                auto bin = std::make_shared<BinaryNode>();
                                bin->str = br.sep;
                                bin->pos = {sep_start, p.seq.rptr};
                                bin->left = std::move(cur);
                                cur = bin;
                                strutil::consume_space(p.seq, true);
                                if (p.seq.match(br.end)) {
                                    goto END;
                                }
                                auto next = p.expr(p);
                                if (!next) {
                                    p.errc.say("expect ", br.op, " next of ", br.sep, " element but not");
                                    p.errc.trace(start, p.seq);
                                    p.err = true;
                                    return true;
                                }
                                bin->right = std::move(next);
                                continue;
                            }
                            break;
                        }
                    END:
                        if (!p.seq.seek_if(br.end)) {
                            p.errc.say("expect ", br.op, " end ", br.end, " but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        auto brac = std::make_shared<BinaryNode>();
                        brac->left = std::move(cur);
                        if (p.tmppass) {
                            brac->right = std::move(p.tmppass);
                        }
                        brac->pos = {start, p.seq.rptr};
                        node = std::move(brac);
                        return true;
                    };
                    (... || fold(br));
                    return node;
                };
            }

            constexpr auto afters(auto expr, auto after) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    auto node = expr(p);
                    if (!node) {
                        return nullptr;
                    }
                    while (true) {
                        p.tmppass = node;
                        p.err = false;
                        auto val = after(p);
                        if (!val) {
                            if (p.err) {
                                return nullptr;
                            }
                            p.tmppass = nullptr;
                            break;
                        }
                        node = val;
                    }
                    return node;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
