/*license*/

// token - define token kind
#pragma once

#include "../wrap/lite/smart_ptr.h"

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

        template <class String>
        struct Token {
            TokenKind kind = TokenKind::root;
            wrap::shared_ptr<Token<String>> next = nullptr;
            Token<String>* prev = nullptr;

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
