/*license*/

// token - define token kind
#pragma once

#include "../wrap/lite/smart_ptr.h"
#include "../wrap/lite/enum.h"

namespace utils {
    namespace tokenize {
        enum class TokenKind {
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
            TokenKind kind = TokenKind::root;

            constexpr Token(TokenKind kind)
                : kind(kind) {}

           public:
            wrap::shared_ptr<Token<String>> next = nullptr;
            Token<String>* prev = nullptr;

            constexpr Token() {}

            const char* what() const {
                return token_kind(kind);
            }

            bool is(TokenKind k) const {
                return kind == k;
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
