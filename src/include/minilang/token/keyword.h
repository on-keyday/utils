/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// keyword - keyword
#pragma once
#include "token.h"
#include "ident.h"
#include "symbol_kind.h"
#include "keyword_kind.h"
#include "ctxbase.h"
#include "tokendef.h"

namespace utils {
    namespace minilang::token {

        template <class T, class Ident>
        constexpr auto yield_keyword_symbol_base(auto&& make, Ident&& ident_judge) {
            return [=](auto word, auto kind) {
                return [=](auto&& src) -> std::shared_ptr<T> {
                    auto trace = trace_log(src, word);
                    size_t b = src.seq.rptr;
                    if (!src.seq.seek_if(word)) {
                        return nullptr;
                    }
                    size_t e = src.seq.rptr;
                    if (ident_judge(src.seq)) {
                        src.seq.rptr = b;
                        return nullptr;
                    }
                    src.seq.rptr = e;
                    auto kwd = make(kind);
                    kwd->token = word;
                    kwd->pos.begin = b;
                    kwd->pos.end = e;
                    return kwd;
                };
            };
        }

        template <class K = KeywordKind, class Ident = decltype(ident_default())>
        constexpr auto yield_keyword(Ident&& ident_judge = ident_default()) {
            return yield_keyword_symbol_base<Keyword<K>>(
                [](K kind) {
                    auto kwd = std::make_shared_for_overwrite<Keyword<K>>();
                    kwd->keyword_kind = kind;
                    return kwd;
                },
                ident_judge);
        }

        template <class K>
        struct WordKind {
            const char* first;
            K second;

            WordKind(const char* w, K k)
                : first(w), second(k) {}
        };

        template <class K = KeywordKind>
        constexpr auto yield_keyword_noident() {
            return yield_keyword_symbol_base<Keyword<K>>(
                [](K kind) {
                    auto kwd = std::make_shared_for_overwrite<Keyword<K>>();
                    kwd->keyword_kind = kind;
                    return kwd;
                },
                [](auto&&) { return false; });
        }

        template <class K = SymbolKind, class Ident = decltype(ident_default())>
        constexpr auto yield_symbol_ident(Ident&& ident_judge = ident_default()) {
            return yield_keyword_symbol_base<Symbol<K>>(
                [](K kind) {
                    auto kwd = std::make_shared_for_overwrite<Symbol<K>>();
                    kwd->symbol_kind = kind;
                    return kwd;
                },
                ident_judge);
        }

        template <class K = SymbolKind>
        constexpr auto yield_symbol() {
            return yield_keyword_symbol_base<Symbol<K>>(
                [](K kind) {
                    auto kwd = std::make_shared_for_overwrite<Symbol<K>>();
                    kwd->symbol_kind = kind;
                    return kwd;
                },
                [](auto&&) { return false; });
        }

        constexpr auto yield_defined_tokens(auto&& make, auto&&... arg) {
            auto fold = [&](auto arg) {
                return make(arg.first, arg.second);
            };
            return yield_oneof(fold(arg)...);
        }

        constexpr auto yield_defined_symbols(auto&& cond) {
            auto make_symbol = yield_symbol();
            auto make = [&](auto word, auto kind) {
                return enabler(make_symbol(word, kind), cond(kind));
            };
            return std::apply(
                [&](auto&&... arg) {
                    return yield_defined_tokens(make, arg...);
                },
                symbols_tuple);
        }

        constexpr auto yield_defined_keywords(auto&& cond) {
            auto make_keyword = yield_keyword();
            auto make = [&](auto word, auto kind) {
                return enabler(make_keyword(word, kind), cond(kind));
            };
            return std::apply(
                [&](auto&&... arg) {
                    return yield_defined_tokens(make, arg...);
                },
                keywords_tuple);
        }

    }  // namespace minilang::token
}  // namespace utils
