/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token_stream - token stream
#pragma once
#include "stream.h"
#include "../../helper/equal.h"

namespace utils {
    namespace parser {
        namespace stream {
            struct SimpleToken {
                const char* tok;
                size_t pos;

                void token(PB pb) {
                    helper::append(pb, tok);
                }

                Error err() {
                    return {};
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
                    return helper::equal(str, "token_err");
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
                        str.push_back(*c);
                        return true;
                    });
                    if (count == 0) {
                        return TokenError{expect, pos};
                    }
                    return SimpleCondToken<Str>{std::move(str), pos, expect};
                }
            };

            template <class Str, class Fn>
            SimpleConditinStream<Str, Fn> make_simplecond(const char* expect, Fn fn) {
                return SimpleConditinStream<Str, Fn>{expect, std::move(fn)};
            }

            struct CharToken {
                std::uint8_t C;
                size_t repeat;
                size_t pos;
                const char* expect;
                void token(PB pb) {
                    pb.push_back(C);
                }

                TokenInfo info() {
                    return TokenInfo{
                        .kind = expect,
                        .pos = pos,
                        .end_child_pos = pos + 1,
                    };
                }
            };

            struct CharError {
                std::uint8_t C;
                size_t pos;
            };

            struct CharStream {
                std::uint8_t C;
                Token parse(Input& input) {
                    size_t pos = input.pos();
                    auto count = input.consume_if([&](const char* c, size_t s, size_t* index) {
                        auto res = consumeStop;
                        while (*index < s && *c == C) {
                            ++*index;
                        }
                        if (*index == s) {
                            res = consumeContinue;
                        }
                        return res;
                    });
                    if (count == 0) {
                        return TokenError{};
                    }
                    return CharToken{
                        .C = C,
                        .repeat = count,
                        .pos = pos,
                        .expect = "char",
                    };
                }
            };

            struct RecoverError {
                Token tok;

                void token(PB) {}
                TokenInfo info() {
                    return TokenInfo{.has_err = true};
                }

                Token child() {
                    return tok.clone();
                }

                Error err() {
                    return std::move(*this);
                }

                void error(PB) {}

                bool kind_of(const char* str) {
                    return helper::equal(str, k(EKind::recover)) ||
                           helper::equal(str, k(EKind::wrapped));
                }

                Error unwrap() {
                    return tok.err();
                }
            };

            template <class Sub>
            struct RecoverStream {
                Sub stream;
                Token parse(Input& input) {
                    auto tok = stream.parse(input);
                    if (has_err(tok)) {
                        return RecoverError{std::move(tok)};
                    }
                    return tok;
                }
            };

        }  // namespace stream
    }      // namespace parser
}  // namespace utils
