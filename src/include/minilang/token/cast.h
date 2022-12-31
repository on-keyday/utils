/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// cast - cast
#pragma once
#include "tokendef.h"

namespace utils {
    namespace minilang::token {

        inline std::shared_ptr<PrimitiveToken> is_Primitive(const std::shared_ptr<Token>& token) {
            if (token && is_primitive(token->kind)) {
                return std::static_pointer_cast<PrimitiveToken>(token);
            }
            return nullptr;
        }

        constexpr Kind make_user_defined_kind(size_t i) {
            return Kind(size_t(Kind::user_defined_) + i);
        }

        template <class Tok, Kind kind>
        constexpr auto make_is_Token() {
            return [](const std::shared_ptr<Token>& token) -> std::shared_ptr<Tok> {
                if (token && token->kind == kind) {
                    return std::static_pointer_cast<Tok>(token);
                }
                return nullptr;
            };
        }

        constexpr auto is_Ident = make_is_Token<Ident, Kind::ident>();
        constexpr auto is_String = make_is_Token<String, Kind::string>();
        constexpr auto is_Number = make_is_Token<Number, Kind::number>();

        template <class K, template <class...> class Tok, Kind kind>
        inline std::shared_ptr<Tok<K>> base_is_Token_template(const std::shared_ptr<Token>& token) {
            constexpr auto of = make_is_Token<Tok<K>, kind>();
            return of(token);
        }

        template <class K = SymbolKind>
        inline auto is_Symbol(const std::shared_ptr<Token>& token) {
            return base_is_Token_template<K, Symbol, Kind::symbol>(token);
        }

        template <class K = KeywordKind>
        inline auto is_Keyword(const std::shared_ptr<Token>& token) {
            return base_is_Token_template<K, Keyword, Kind::keyword>(token);
        }

        template <class K>
        inline std::shared_ptr<Symbol<K>> is_SymbolKindof(const std::shared_ptr<Token>& token, K kind) {
            if (auto sym = is_Symbol<K>(token); sym && sym->symbol_kind == kind) {
                return sym;
            }
            return nullptr;
        }

        template <class K>
        inline std::shared_ptr<Keyword<K>> is_KeywordKindof(const std::shared_ptr<Token>& token, K kind) {
            if (auto sym = is_Keyword<K>(token); sym && sym->keyword_kind == kind) {
                return sym;
            }
            return nullptr;
        }

        constexpr auto is_Space = make_is_Token<Space, Kind::space>();
        constexpr auto is_Line = make_is_Token<Line, Kind::line>();
        constexpr auto is_Comment = make_is_Token<Comment, Kind::comment>();

        constexpr auto is_Bof = make_is_Token<Bof, Kind::bof>();
        constexpr auto is_Eof = make_is_Token<Eof, Kind::eof>();

        constexpr auto is_UnaryTree = make_is_Token<UnaryTree, Kind::unary_tree>();
        constexpr auto is_BinTree = make_is_Token<BinTree, Kind::bin_tree>();
        constexpr auto is_Brackets = make_is_Token<Brackets, Kind::brackets>();

        template <template <class...> class Vec = std::vector>
        inline std::shared_ptr<Block<Vec>> is_Block(const std::shared_ptr<Token>& token) {
            constexpr auto of = make_is_Token<Block<Vec>, Kind::block>();
            return of(token);
        }

        constexpr auto symbol_of(auto f, auto... k) {
            return [=](const std::shared_ptr<Token>& tok) {
                if (!tok || tok->kind != Kind::symbol) {
                    return false;
                }
                auto ptr = static_cast<Symbol<decltype(f)>*>(tok.get());
                return ptr->symbol_kind == f || (... || (ptr->symbol_kind == k));
            };
        }

        void tree_to_vec(auto&& vec, const std::shared_ptr<Token>& tok, auto&& cond) {
            if (!tok) {
                return;
            }
            auto tree = is_BinTree(tok);
            if (!tree) {
                vec.push_back(tok);
                return;
            }
            if (cond(tree->symbol)) {
                tree_to_vec(vec, tree->left, cond);
                tree_to_vec(vec, tree->right, cond);
                return;
            }
            vec.push_back(tok);
        }

    }  // namespace minilang::token
}  // namespace utils
