/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// tokendef - token definition
#pragma once
#include "token.h"
#include "../../number/parse.h"
#include "symbol_kind.h"
#include "keyword_kind.h"
#include <vector>

namespace utils {
    namespace minilang::token {
        struct Comment : PrimitiveToken {
            Comment()
                : PrimitiveToken(Kind::comment) {}
            std::string detail;
            std::string begin;
            std::string end;
        };

        struct Ident : PrimitiveToken {
            Ident()
                : PrimitiveToken(Kind::ident) {}
            std::weak_ptr<Token> first_lookup;
            std::weak_ptr<Token> second_lookup;
        };

        struct String : PrimitiveToken {
            String()
                : PrimitiveToken(Kind::string) {}
            number::NumErr unesc_err;
            std::string unescaped;
            int prefix;
        };

        struct Space : PrimitiveToken {
            Space()
                : PrimitiveToken(Kind::space) {}
            char16_t space;
            size_t count;
        };

        struct Line : PrimitiveToken {
            Line()
                : PrimitiveToken(Kind::line) {}
            bool lf;
            bool cr;
            size_t count;
        };

        struct Eof : Token {
            Eof()
                : Token(Kind::eof) {}
        };

        struct Bof : Token {
            Bof()
                : Token(Kind::bof) {}
        };

        template <class K = KeywordKind>
        struct Keyword : PrimitiveToken {
            Keyword()
                : PrimitiveToken(Kind::keyword) {}
            K keyword_kind;
        };

        template <class K = SymbolKind>
        struct Symbol : PrimitiveToken {
            Symbol()
                : PrimitiveToken(Kind::symbol) {}
            K symbol_kind;
        };

        struct Number : PrimitiveToken {
            Number()
                : PrimitiveToken(Kind::number) {}
            std::string num_str;
            bool is_float;
            number::NumErr int_err;
            std::uint64_t integer;
            number::NumErr float_err;
            double float_;
            int radix;
        };

        struct UnaryTree : Token {
            std::shared_ptr<Token> symbol;
            std::shared_ptr<Token> target;
            UnaryTree()
                : Token(Kind::unary_tree) {}
        };

        struct BinTree : Token {
            std::shared_ptr<Token> symbol;
            std::shared_ptr<Token> left;
            std::shared_ptr<Token> right;
            BinTree()
                : Token(Kind::bin_tree) {}
        };

        struct Brackets : Token {
            std::shared_ptr<Token> callee;
            std::shared_ptr<Token> left;
            std::shared_ptr<Token> center;
            std::shared_ptr<Token> right;
            Brackets()
                : Token(Kind::brackets) {}
        };

        template <template <class...> class Vec = std::vector>
        struct Block : Token {
            std::shared_ptr<Token> begin;
            Vec<std::shared_ptr<Token>> elements;
            std::shared_ptr<Token> end;
            Block()
                : Token(Kind::block) {}
        };
    }  // namespace minilang::token
}  // namespace utils
