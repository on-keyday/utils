/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../internal/opti.h"
#include "../status.h"
#include "../../core/sequencer.h"
#include <ranges>

namespace utils::comb2 {
    namespace types {
        template <class Lit>
        struct Literal {
            Lit literal;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                if constexpr (std::is_integral_v<Lit>) {
                    if (!seq.consume_if(literal)) {
                        return Status::not_match;
                    }
                }
                else {
                    if (!seq.seek_if(literal)) {
                        return Status::not_match;
                    }
                }
                return Status::match;
            }
        };

        template <class Lit>
        struct OneOfLiteral {
            Lit literal;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                if constexpr (std::is_integral_v<Lit>) {
                    if (!seq.consume_if(literal)) {
                        return Status::not_match;
                    }
                }
                else if constexpr (std::is_pointer_v<Lit>) {
                    auto len = utils::strlen(literal);
                    for (auto i = 0; i < len; i++) {
                        if (seq.consume_if(literal[i])) {
                            return Status::match;
                        }
                    }
                    return Status::not_match;
                }
                else if constexpr (std::ranges::range<Lit>) {
                    for (auto c : literal) {
                        if (seq.consume_if(c)) {
                            return Status::match;
                        }
                    }
                    return Status::not_match;
                }
                else {
                    auto len = literal.size();
                    for (auto i = 0; i < len; i++) {
                        if (seq.consume_if(literal[i])) {
                            return Status::match;
                        }
                    }
                    return Status::not_match;
                }
            }
        };

        template <class LitA, class LitB>
        struct RangeLiteral {
            LitA lita;
            LitB litb;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                if (seq.current() < lita ||
                    seq.current() > litb) {
                    return Status::not_match;
                }
                seq.consume();
                return Status::match;
            }
        };

        // match to end of sequence
        struct EOS {
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                if (!seq.eos()) {
                    return Status::not_match;
                }
                return Status::match;
            }
        };

        // match to begin of sequence
        struct BOS {
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                if (seq.rptr != 0) {
                    return Status::not_match;
                }
                return Status::match;
            }
        };

        // match to begin of line
        struct BOL {
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                if (seq.rptr != 0 &&
                    seq.current(-1) != '\r' &&
                    seq.current(-1) != '\n') {
                    return Status::not_match;
                }
                return Status::match;
            }
        };

        struct Null {
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                return Status::match;
            }
        };
    }  // namespace types

    namespace ops {
        constexpr auto eos = types::EOS{};
        constexpr auto bos = types::BOS{};
        constexpr auto bol = types::BOL{};

        constexpr auto null = types::Null{};

        template <class Lit>
        constexpr auto lit(Lit&& lit) {
            return types::Literal<std::decay_t<Lit>>{std::forward<Lit>(lit)};
        }

        template <class Lit>
        constexpr auto oneof(Lit&& lit) {
            return types::OneOfLiteral<std::decay_t<Lit>>{std::forward<Lit>(lit)};
        }

        template <class A, class B>
        constexpr auto range(A&& a, B&& b) {
            return types::RangeLiteral<std::decay_t<A>, std::decay_t<B>>{std::forward<A>(a), std::forward<B>(b)};
        }

        constexpr auto operator""_l(const char* s, size_t) {
            return lit(s);
        }

        constexpr auto operator""_l(char s) {
            return lit(s);
        }

        constexpr auto operator""_oo(const char* s, size_t) {
            return oneof(s);
        }
    }  // namespace ops
}  // namespace utils::comb2
