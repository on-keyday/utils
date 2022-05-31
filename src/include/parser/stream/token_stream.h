/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token_stream - token stream
#pragma once
#include "stream.h"
#include "equal.h"

namespace utils {
    namespace parser {
        namespace stream {
            struct SimpleToken {
                const char* tok;
                size_t pos;

                void token(PB pb) {
                    helper::append(pb, tok);
                }

                TokenInfo info() {
                    return TokenInfo{
                        .kind = tok,
                        .pos = pos,
                        .end_child_pos = pos + ::strlen(tok),
                    };
                }
            };

            struct TokenError {
                const char* expected;
                size_t pos;
                void token(PB pb) {
                    helper::appends(pb, "expect token `", expected, "` but not matched");
                }

                TokenInfo info() {
                    return TokenInfo{
                        .pos = pos,
                        .has_err = true,
                    };
                }

                Error err() {
                    return *this;
                }

                void error(PB pb) {
                    token(pb);
                }

                bool kind_of(const char* str) {
                    return helper::default_equal(str, "token_err");
                }
            };

            struct SimpleTokenStream {
                const char* tok;
                Token parse(Input& input) {
                    auto pos = input.pos();
                    if (!input.consume(tok)) {
                        return TokenError{tok, pos};
                    }
                    return SimpleToken{tok, pos};
                }
            };

            template <class Str>
            struct SimpleCondToken {
                Str tok;
                size_t pos;
                const char* expected;
                void token(PB pb) {
                    helper::append(pb, tok);
                }

                TokenInfo info() {
                    return TokenInfo{
                        .kind = expected,
                        .pos = pos,
                        .end_child_pos = pos + tok.size(),
                    };
                }
            };

            template <class Str, class Fn>
            struct SimpleConditinStream {
                const char* expect;
                Fn fn;
                Token parse(Input& input) {
                    auto pos = input.pos();
                    Str str;
                    auto count = input.consume_if([&](const char* c) {
                        if (!fn(c)) {
                            return false;
                        }
                        str.push_back(c);
                        return true;
                    });
                    if (count == 0) {
                        return TokenError{expect, pos};
                    }
                }
            };

        }  // namespace stream
    }      // namespace parser
}  // namespace utils
