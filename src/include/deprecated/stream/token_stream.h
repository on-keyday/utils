/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token_stream - token stream
#pragma once
#include "stream.h"
#include <helper/equal.h>

namespace utils {
    namespace parser {
        namespace stream {

            constexpr auto tokenSimple = "simple";

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
                        .kind = tokenSimple,
                        .dirtok = tok,
                        .pos = pos,
                        .len = ::strlen(tok),
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

            constexpr auto tokenSimpleCond = "simple_cond";

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
                        .kind = tokenSimpleCond,
                        .dirtok = tok.c_str(),
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
                bool zero_is_not_error;

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
                        if (zero_is_not_error) {
                            return {};
                        }
                        return TokenError{"char", pos};
                    }
                    return CharToken{
                        .C = C,
                        .repeat = count,
                        .pos = pos,
                        .expect = "char",
                    };
                }
            };

            CharStream make_char(std::uint8_t c, bool zero_is_not_error = false) {
                return CharStream{c, zero_is_not_error};
            }

            struct RecoverError {
                Token tok;

                void token(PB) {}
                TokenInfo info() {
                    return TokenInfo{.has_err = true};
                }

                Token child(size_t i) {
                    if (i == 0) {
                        return tok.copy();
                    }
                    return {};
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

            constexpr auto tokenTrimed = "trimed";

            struct TrimedToken {
                Token tok;
                Token trim;
                bool after;
                void token(PB pb) {}
                TokenInfo info() {
                    TokenInfo info{};
                    info.kind = tokenTrimed;
                    if (after) {
                        info.pos = tok.pos();
                    }
                    else {
                        info.pos = trim.pos();
                    }
                    info.fixed_child = true;
                    info.child = 2;
                    return info;
                }

                Token copy() {
                    return TrimedToken{tok.copy(), trim.copy(), after};
                }
            };

            template <bool after = false, class Sub>
            Token trimming(Sub& other, Input& input) {
                auto trim = get_trimminger(input);
                if (!trim) {
                    return other.parse(input);
                }
                if constexpr (!after) {
                    auto trimed = trim->parse(input);
                    if (has_err(trimed)) {
                        return trimed;
                    }
                    auto tok = other.parse(input);
                    if (has_err(tok)) {
                        return tok;
                    }
                    if (trimed == nullptr) {
                        return tok;
                    }
                    return TrimedToken{std::move(tok), std::move(trimed), false};
                }
                else {
                    auto tok = other.parse(input);
                    if (has_err(tok)) {
                        return tok;
                    }
                    auto trimed = trim->parse(input);
                    if (has_err(trimed)) {
                        return trimed;
                    }
                    if (trimed == nullptr) {
                        return tok;
                    }
                    return TrimedToken{std::move(tok), std::move(trimed), true};
                }
            }

            template <bool after, class Sub>
            struct TrimingStream {
                Sub other;
                Token parse(Input& input) {
                    return trimming<after>(other, input);
                }
            };

            template <bool after = false>
            auto make_trimming(auto&& other) {
                return TrimingStream<after, std::remove_cvref_t<decltype(other)>>{std::move(other)};
            }

            auto make_bothtrimming(auto&& other) {
                return make_trimming<false>(make_trimming<true>(std::move(other)));
            }

        }  // namespace stream
    }      // namespace parser
}  // namespace utils
