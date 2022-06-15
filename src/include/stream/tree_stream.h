/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tree_stream - tree stream
#pragma once
#include "stream.h"
#include <helper/equal.h>

namespace utils {
    namespace parser {
        namespace stream {
            constexpr auto tokenBinary = "binary";

            struct BinaryToken {
                const char* tok;
                Token left;
                Token right;
                size_t pos;
                void token(PB pb) {
                    helper::append(pb, tok);
                }

                TokenInfo info() {
                    return TokenInfo{
                        .kind = tokenBinary,
                        .dirtok = tok,
                        .pos = pos,
                        .len = ::strlen(tok),
                        .child = 2,
                        .fixed_child = true,
                    };
                }

                Token child(size_t i) {
                    if (i == 0) {
                        return left.clone();
                    }
                    else if (i == 1) {
                        return right.clone();
                    }
                    return {};
                }

                Token copy() {
                    return BinaryToken{
                        tok,
                        left.clone(),
                        right.clone(),
                        pos,
                    };
                }
            };

            struct Expect {
                const char* expect;

                bool ok(Input&) const {
                    return true;
                }
            };

            template <class Fn>
            struct ExpectWith {
                const char* expect;
                Fn ok;

                constexpr ExpectWith(const char* e, Fn o)
                    : expect(e), ok(std::move(o)) {}
            };

            auto expects(auto... o) {
                auto expect_one = [](Input& input, auto&& e, auto& ref, size_t& pos) {
                    pos = input.pos();
                    if (!input.consume(e.expect)) {
                        return false;
                    }
                    if (!e.ok(input)) {
                        input.seek(pos);
                        return false;
                    }
                    ref = e.expect;
                    return true;
                };
                return [=](Input& input, auto& ref, size_t& pos) {
                    return (... || expect_one(input, o, ref, pos));
                };
            }

            struct AfterTokenError {
                Token intok;
                const char* tok;
                size_t pos;
                void token(PB) {}
                void error(PB pb) {
                    helper::appends(pb, "after token `", tok, "`");
                }

                Error err() {
                    return AfterTokenError{intok.clone(), tok};
                }

                TokenInfo info() {
                    return TokenInfo{.has_err = true};
                }

                Token copy() {
                    return AfterTokenError{intok.clone(), tok, pos};
                }

                Error unwrap() {
                    return intok.err();
                }

                bool kind_of(const char* key) {
                    return helper::equal(key, k(EKind::wrapped));
                }
            };

            template <class Sub, class Expecter, bool rentrant>
            struct BinaryStream {
                Sub one;
                Expecter expect;

                Stream copy() {
                    auto cpy = one;
                    return BinaryStream<Sub, Expecter, rentrant>{std::move(cpy), expect};
                }

                BinaryStream(Sub&& st, Expecter exp)
                    : one(std::move(st)), expect(std::move(exp)) {}

                Token parse(Input& input) {
                    auto ret = one.parse(input);
                    if (has_err(ret)) {
                        return ret;
                    }
                    const char* expected = nullptr;
                    size_t pos = input.pos();
                    while (expect(input, expected, pos)) {
                        Token right;
                        if constexpr (rentrant) {
                            right = parse(input);
                        }
                        else {
                            right = one.parse(input);
                        }
                        if (has_err(right)) {
                            return AfterTokenError{std::move(right), expected, pos};
                        }
                        ret = std::move(BinaryToken{
                            .tok = expected,
                            .left = std::move(ret),
                            .right = std::move(right),
                            .pos = pos,

                        });
                    }
                    return ret;
                }
            };

            template <class Sub>
            auto make_binary(Sub one, auto&&... o) {
                return BinaryStream<Sub, decltype(expects(o...)), false>{std::move(one), expects(o...)};
            }

            template <class Sub>
            auto make_assign(Sub one, auto&&... o) {
                return BinaryStream<Sub, decltype(expects(o...)), true>{std::move(one), expects(o...)};
            }

            constexpr auto tokenUnary = "unary";

            struct UnaryToken {
                const char* tok;
                Token target;
                size_t pos;
                void token(PB pb) {
                    helper::append(pb, tok);
                }

                TokenInfo info() {
                    TokenInfo info{};
                    info.kind = tokenUnary;
                    info.dirtok = tok;
                    info.pos = pos;
                    info.len = ::strlen(tok);
                    info.child = 2;
                    info.fixed_child = true;
                    return info;
                }

                Token child(size_t i) {
                    if (i == 0) {
                        return target.clone();
                    }
                    return {};
                }

                Token copy() {
                    return UnaryToken{tok, target.clone(), pos};
                }
            };

            template <class Sub, class Expecter, bool rentrant>
            struct UnaryStream {
                Sub one;
                Expecter expect;

                Stream copy() {
                    auto cpy = one;
                    return UnaryStream<Sub, Expecter, rentrant>{std::move(cpy), expect};
                }

                UnaryStream(Sub&& o, Expecter e)
                    : one(std::move(o)), expect(std::move(e)) {}

                Token parse(Input& input) {
                    const char* expected = nullptr;
                    size_t pos = 0;
                    Token trimed;
                    if (auto trim = get_trimminger(input)) {
                        pos = input.pos();
                        trimed = trim->parse(input);
                        if (has_err(trimed)) {
                            return trimed;
                        }
                    }
                    if (expect(input, expected, pos)) {
                        Token target;
                        if (rentrant) {
                            target = parse(input);
                        }
                        else {
                            target = one.parse(input);
                        }
                        if (has_err(target)) {
                            return AfterTokenError{std::move(target), expected, pos};
                        }
                        if (trimed != nullptr) {
                            return TrimedToken{UnaryToken{expected, std::move(target), pos}, std::move(trimed), false};
                        }
                    }
                    if (trimed != nullptr) {
                        input.seek(pos);
                    }
                    return one.parse(input);
                }
            };

            template <bool rentrant = false, class Sub>
            auto make_unary(Sub target, auto&&... o) {
                return UnaryStream<Sub, decltype(expects(o...)), rentrant>{std::move(target), expects(o...)};
            }

            struct ExpectCast {
                const char* expect;
                bool ok_before(Input&) {
                    return true;
                }
                bool rollback_before;

                Token ok_after(Input&) {
                    return {};
                }
            };

            constexpr auto tokenCast = "cast";

            struct CastToken {
                Token incast;
                Token aftertoken;
                const char* tok;
                size_t pos;

                void token(PB pb) {
                    helper::append(pb, tok);
                }

                TokenInfo info() {
                    TokenInfo info{};
                    info.kind = tokenCast;
                    info.dirtok = tok;
                    info.pos = pos;
                    info.len = ::strlen(tok);
                    info.child = 2;
                    info.fixed_child = true;
                    return info;
                }

                Token chlid(size_t i) {
                    if (i == 0) {
                        return incast.clone();
                    }
                    if (i == 1) {
                        return aftertoken.clone();
                    }
                    return {};
                }

                Token copy() {
                    return CastToken{incast.clone(), aftertoken.clone(), tok, pos};
                }
            };

            template <class Expecter>
            struct CastStream {
                Stream other;
                Stream innercast;
                Expecter expecter;

                Stream copy() {
                    return CastStream<Expecter>{other.clone(), innercast.clone(), expecter};
                }

                Token parse(Input& input) {
                    auto pos = input.pos();
                    if (!input.consume(expecter.expect)) {
                        return other.parse(input);
                    }
                    if (!expecter.ok_before(input)) {
                        if (expecter.rollback_before) {
                            input.seek(pos);
                            return other.parse(input);
                        }
                    }
                    auto incast = innercast.parse(input);
                    if (has_err(incast)) {
                        return AfterTokenError{std::move(incast), expecter.expect, pos};
                    }
                    auto after = expecter.ok_after(input);
                    if (has_err(after)) {
                        return AfterTokenError{std::move(after), expecter.expect, pos};
                    }
                    return CastToken{std::move(incast), std::move(after), expecter.expect, pos};
                }
            };

            template <class Expecter>
            auto make_cast(Stream other, Stream incast, Expecter expecter) {
                return CastStream<Expecter>{std::move(other), std::move(incast), expecter};
            }
        }  // namespace stream
    }      // namespace parser
}  // namespace utils
