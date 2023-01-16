/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <minilang/token/parsers.h>
#include "symbols.h"

namespace vstack {
    namespace token = utils::minilang::token;
    using s = token::SymbolKind;
    using k = token::KeywordKind;

    constexpr auto get_token = token::skip_spaces({})(token::yield_single_token(true));

    struct Func : token::Token {
        std::shared_ptr<token::Token> fn_keyword;
        std::shared_ptr<token::Ident> ident;
        std::shared_ptr<token::Symbol<>> brackets_begin;
        std::vector<std::shared_ptr<token::Ident>> args;
        std::shared_ptr<token::Symbol<>> brackets_end;
        std::shared_ptr<token::Block<>> block;
        symbol::VstackFn vstack_fn;

        Func()
            : Token(token::Kind::func_) {}
    };

    constexpr auto is_Func = token::make_is_Token<Func, token::Kind::func_>();

    constexpr auto fn_def(auto&& inner) {
        return [=](auto&& src) -> std::shared_ptr<Func> {
            if (!token::load_next(src)) {
                return nullptr;
            }
            size_t be = src.seq.rptr;
            std::shared_ptr<token::Keyword<>> fn_key = token::is_KeywordKindof(src.next, k::fn_);
            if (!fn_key) {
                return nullptr;
            }
            auto fn = std::make_shared_for_overwrite<Func>();
            fn->fn_keyword = std::move(fn_key);
            src.next = nullptr;
            src.ctx.is_def = true;
            std::shared_ptr<token::Token> tok = get_token(src);
            fn->ident = token::is_Ident(tok);
            if (!fn->ident) {
                token::error_log(src, "expect identifier but not");
                token::token_log(src, tok);
                src.err = true;
                return nullptr;
            }
            auto current_scope = src.ctx.refs.cur;
            current_scope->no_discard = true;
            auto parent_scope = current_scope->parent.lock();
            fn->ident->first_lookup = parent_scope->define_func(fn->ident->token, fn);
            tok = get_token(src);
            fn->brackets_begin = token::is_SymbolKindof(tok, s::parentheses_begin);
            if (!fn->brackets_begin) {
                token::error_log(src, "expect function begin (");
                token::token_log(src, tok);
                src.err = true;
                return nullptr;
            }
            while (true) {
                tok = get_token(src);
                if (fn->brackets_end = token::is_SymbolKindof(tok, s::parentheses_end); fn->brackets_end) {
                    break;
                }
                auto arg = token::is_Ident(tok);
                if (!arg) {
                    token::error_log(src, "expect identifier but not");
                    token::token_log(src, arg);
                    src.err = true;
                    return nullptr;
                }
                arg->first_lookup = current_scope->define_var(arg->token, arg);
                fn->args.push_back(std::move(arg));
                tok = get_token(src);
                if (fn->brackets_end = token::is_SymbolKindof(tok, s::parentheses_end);
                    fn->brackets_end) {
                    break;
                }
                if (!token::is_SymbolKindof(tok, s::comma)) {
                    token::error_log(src, "expect , but not");
                    token::token_log(src, tok);
                    src.err = true;
                    return nullptr;
                }
            }
            src.ctx.is_def = false;
            constexpr auto skip_begin = token::skip_spaces({.space = true, .line = false, .comment = false});
            constexpr auto skip_end = token::skip_spaces({});
            constexpr auto peek_token = skip_begin(token::yield_single_token(false));
            auto peek = peek_token(src);
            if (token::is_SymbolKindof(peek, s::braces_begin)) {
                auto block = token::yield_block(token::symbol_expector(true)(s::braces_begin), inner, skip_end(token::symbol_expector(true)(s::braces_end)));
                fn->block = symbol::block_scope(block)(src);
                if (!fn->block) {
                    token::error_log(src, "expect { but not. library bug!!");
                    token::token_log(src, peek);
                    src.err = true;
                    return nullptr;
                }
            }
            fn->pos.begin = be;
            fn->pos.end = src.seq.rptr;
            return fn;
        };
    }
    /*
        let a,b,c,d = 2,3,4,6
     */
    struct Let : token::Token {
        Let()
            : Token(token::Kind::var_) {}
        std::shared_ptr<token::Keyword<>> let_keyword;
        std::vector<std::shared_ptr<token::Ident>> idents;
        std::shared_ptr<token::Symbol<>> assign;
        std::shared_ptr<token::Token> init;
    };

    constexpr auto is_Let = token::make_is_Token<Let, token::Kind::var_>();

    constexpr auto let_def(auto&& init) {
        return [=](auto&& src) -> std::shared_ptr<Let> {
            if (!token::load_next(src)) {
                return nullptr;
            }
            size_t be = src.seq.rptr;
            std::shared_ptr<token::Keyword<>> let_key = token::is_KeywordKindof(src.next, k::let_);
            if (!let_key) {
                return nullptr;
            }
            auto let = std::make_shared<Let>();
            let->let_keyword = std::move(let_key);
            src.next = nullptr;
            src.ctx.is_def = true;
            std::shared_ptr<token::Token> tok;
            size_t ptr;
            while (true) {
                tok = get_token(src);
                auto ident = token::is_Ident(tok);
                if (!ident) {
                    token::error_log(src, "expect identifier");
                    token::token_log(src, tok);
                    src.err = true;
                    return nullptr;
                }
                ident->first_lookup = src.ctx.refs.cur->define_var(ident->token, ident);
                let->idents.push_back(std::move(ident));
                ptr = src.seq.rptr;
                tok = get_token(src);
                if (auto sym = token::is_SymbolKindof(tok, s::comma)) {
                    continue;
                }
                break;
            }
            src.ctx.is_def = false;
            if (let->assign = token::is_SymbolKindof(tok, s::assign);
                let->assign) {
                let->init = init(src);
                if (!let->init) {
                    token::error_log(src, "expect initializer");
                    token::token_log(src, let->let_keyword);
                    src.err = true;
                    return nullptr;
                }
            }
            else {
                src.seq.rptr = ptr;
            }
            let->pos.begin = be;
            let->pos.end = src.seq.rptr;
            return let;
        };
    }

    namespace func {
        void walk(const std::shared_ptr<token::Token>& tok);
    }
}  // namespace vstack
