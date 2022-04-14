/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token - define token kind
#pragma once

#include "../wrap/light/smart_ptr.h"
#include "../wrap/light/enum.h"
#include <cstdint>

namespace utils {
    namespace tokenize {
        enum class TokenKind : std::uint8_t {
            space,
            line,
            keyword,
            context,
            symbol,
            identifier,
            comment,
            root,
            unknown,
        };

        BEGIN_ENUM_STRING_MSG(TokenKind, token_kind)
        ENUM_STRING_MSG(TokenKind::space, "space")
        ENUM_STRING_MSG(TokenKind::line, "line")
        ENUM_STRING_MSG(TokenKind::keyword, "keyword")
        ENUM_STRING_MSG(TokenKind::context, "context_keyword")
        ENUM_STRING_MSG(TokenKind::symbol, "symbol")
        ENUM_STRING_MSG(TokenKind::identifier, "identifier")
        ENUM_STRING_MSG(TokenKind::comment, "comment")
        ENUM_STRING_MSG(TokenKind::root, "root")
        END_ENUM_STRING_MSG("unknown")

        template <class String>
        struct Token {
           protected:
            TokenKind kind_ = TokenKind::root;

            constexpr Token(TokenKind kind)
                : kind_(kind) {}

           public:
            wrap::shared_ptr<Token<String>> next = nullptr;
            Token<String>* prev = nullptr;

            constexpr Token() {}

            TokenKind kind() const {
                return kind_;
            }

            const char* what() const {
                return token_kind(kind_);
            }

            bool is(TokenKind k) const {
                return kind_ == k;
            }

            virtual bool has(const String&) const {
                return false;
            }

            virtual size_t layer() const {
                return 0;
            }

            virtual String to_string() const {
                return String();
            }
        };

    }  // namespace tokenize
}  // namespace utils
