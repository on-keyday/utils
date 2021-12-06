/*license*/

// token - define token kind
#pragma once

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
        };

        template <class String>
        struct Space : Token<String> {
        };
    }  // namespace tokenize
}  // namespace utils
