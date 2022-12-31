/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// tree - concept group
#pragma once
#include "token.h"
#include "keyword.h"

namespace utils {
    namespace minilang::token {

        constexpr auto expect_then(auto&& next, auto&& expect) {
            return [=](auto&& src) -> std::shared_ptr<Token> {
                const auto trace = trace_log(src, "expect_then");
                src.err = false;
                size_t b = src.seq.rptr;
                std::shared_ptr<Token> symbol = expect(src);
                if (!symbol) {
                    return nullptr;
                }
                std::shared_ptr<Token> target = next(src);
                if (!target) {
                    error_log(src, "expect right hand of symbol");
                    token_log(src, symbol);
                    src.err = true;
                    return nullptr;
                }
                auto ur = std::make_shared_for_overwrite<UnaryTree>();
                ur->pos.begin = b;
                ur->pos.end = src.seq.rptr;
                ur->symbol = std::move(symbol);
                ur->target = std::move(target);
                return ur;
            };
        }

        constexpr auto unary(auto&& next, auto&& expect, bool self_call = false) {
            auto f = [=](auto&& f, auto&& src) -> std::shared_ptr<Token> {
                const auto trace = trace_log(src, "unary");
                src.err = false;
                size_t b = src.seq.rptr;
                if (std::shared_ptr<Token> symbol = expect(src)) {
                    std::shared_ptr<Token> target = self_call ? f(f, src) : next(src);
                    if (!target) {
                        error_log(src, "expect right hand of operator");
                        token_log(src, symbol);
                        src.err = true;
                        return nullptr;
                    }
                    auto ur = std::make_shared_for_overwrite<UnaryTree>();
                    ur->pos.begin = b;
                    ur->pos.end = src.seq.rptr;
                    ur->symbol = std::move(symbol);
                    ur->target = std::move(target);
                    return ur;
                }
                if (src.err) {
                    return nullptr;
                }
                return next(src);
            };
            return [=](auto&& src) -> std::shared_ptr<Token> {
                return f(f, src);
            };
        }

        constexpr auto binary_base(auto log_msg, auto&& next, auto&& impl) {
            auto f = [=](auto&& f, auto&& src) -> std::shared_ptr<Token> {
                const auto trace = trace_log(src, log_msg);
                auto self = [&](auto&& src) {
                    return f(f, src);
                };
                size_t b = src.seq.rptr;
                std::shared_ptr<Token> tok = next(src);
                if (!tok) {
                    return nullptr;
                }
                for (std::shared_ptr<Token> new_tok = impl(src, tok, b, next, self); new_tok; new_tok = impl(src, tok, b, next, self)) {
                    tok = std::move(new_tok);
                }
                if (src.err) {
                    return nullptr;
                }
                return tok;
            };
            return [=](auto&& src) -> std::shared_ptr<Token> {
                return f(f, src);
            };
        }

        constexpr auto adapt_after(auto&& expect_after) {
            return [=](auto&& src, std::shared_ptr<Token>& prev, size_t b, auto&& inner, auto& self) {
                return expect_after(src, prev, b);
            };
        }

        constexpr auto binary_after(auto&& next, auto&& expect_after) {
            return binary_base("binary_ex", next, expect_after);
        }

        constexpr auto binary_right(auto&& expect, bool self_call = false) {
            return [=](auto&& src, std::shared_ptr<Token>& prev, size_t b, auto&& inner, auto& self) -> std::shared_ptr<Token> {
                std::shared_ptr<Token> sym = expect(src);
                if (!sym) {
                    return nullptr;
                }
                std::shared_ptr<Token> right = self_call ? self(src) : inner(src);
                if (!right) {
                    error_log(src, "expect right hand of operator");
                    token_log(src, sym);
                    src.err = true;
                    return nullptr;
                }
                auto tree = std::make_shared_for_overwrite<BinTree>();
                tree->pos.begin = b;
                tree->pos.end = src.seq.rptr;
                tree->left = std::move(prev);
                tree->right = std::move(right);
                tree->symbol = std::move(sym);
                return tree;
            };
        }

        constexpr auto adapt_expect(auto&& expect_after) {
            return [=](auto&& src) {
                size_t b = src.seq.rptr;
                std::shared_ptr<Token> prev;
                return expect_after(src, prev, b);
            };
        }

        constexpr auto binary(auto&& next, auto&& expect, bool self_call = false) {
            return binary_base("binary", next, binary_right(expect, self_call));
        }

        constexpr auto asynmetric(auto&& left, auto&& right, auto&& expect) {
            auto asyn = [=](auto&& src, std::shared_ptr<Token>& prev, size_t b) {
                std::shared_ptr<Token> sym = expect(src);
                if (!sym) {
                    return nullptr;
                }
                std::shared_ptr<Token> r = right(src);
                if (!r) {
                    error_log(src, "expect right hand of operator");
                    token_log(src, sym);
                    src.err = true;
                    return nullptr;
                }
                auto tree = std::make_shared_for_overwrite<BinTree>();
                tree->pos.begin = b;
                tree->pos.end = src.seq.rptr;
                tree->left = std::move(prev);
                tree->right = std::move(r);
                tree->symbol = std::move(sym);
                return tree;
            };
            return binary_base("asynmetric_tree", left, adapt_binbase(asyn));
        }

        constexpr auto symbol_expector(bool should_move = true) {
            return [=](auto f, auto... k) {
                return [=](auto&& src) -> std::shared_ptr<Token> {
                    if (!load_next(src)) {
                        return nullptr;
                    }
                    if (src.next->kind != Kind::symbol) {
                        return nullptr;
                    }
                    using F = decltype(f);
                    Symbol<F>* ptr = static_cast<Symbol<F>*>(src.next.get());
                    auto fold = [&](auto k) {
                        static_assert(std::is_same_v<F, decltype(k)>);
                        return ptr->symbol_kind == k;
                    };
                    if (fold(f) || (... || fold(k))) {
                        if (should_move) {
                            auto tok = std::move(src.next);
                            return tok;
                        }
                        else {
                            return src.next;
                        }
                    }
                    return nullptr;
                };
            };
        }

        constexpr auto keyword_expector(bool should_move = true) {
            return [=](auto f, auto... k) {
                return [=](auto&& src) -> std::shared_ptr<Token> {
                    if (!load_next(src)) {
                        return nullptr;
                    }
                    if (src.next->kind != Kind::keyword) {
                        return nullptr;
                    }
                    using F = decltype(f);
                    Keyword<F>* ptr = static_cast<Keyword<F>*>(src.next.get());
                    auto fold = [&](auto k) {
                        static_assert(std::is_same_v<F, decltype(k)>);
                        return ptr->keyword_kind == k;
                    };
                    if (fold(f) || (... || fold(k))) {
                        if (should_move) {
                            auto tok = std::move(src.next);
                            return tok;
                        }
                        else {
                            return src.next;
                        }
                    }
                    return nullptr;
                };
            };
        }

    }  // namespace minilang::token
}  // namespace utils
