/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tree_stream - tree stream
#pragma once
#include "stream.h"
#include "../../helper/equal.h"

namespace utils {
    namespace parser {
        namespace stream {
            struct BinaryToken {
                const char* tok;
                Token left;
                Token right;
                void token(PB pb) {
                    helper::append(pb, tok);
                }

                Token copy() {
                    return BinaryToken{
                        tok,
                        left.clone(),
                        right.clone(),
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

            template <class Expecter, bool rentrant>
            struct BinaryStream {
                Stream one;
                Expecter expect;

                BinaryStream(Stream&& st, Expecter exp)
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
                        if constexpr (reentrant) {
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
                        });
                    }
                    return ret;
                }
            };

            auto make_binary(Stream one, auto&&... o) {
                return BinaryStream<decltype(expects(o...)), false>{std::move(one), expects(o...)};
            }

            auto make_assign(Stream one, auto&&... o) {
                return BinaryStream<decltype(expects(o...)), true>{std::move(one), expects(o...)};
            }
        }  // namespace stream
    }      // namespace parser
}  // namespace utils
