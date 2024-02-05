/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../status.h"
#include "../../unicode/utf/convert.h"
#include "../internal/context_fn.h"
#include "../internal/test.h"

namespace futils::comb2 {
    namespace types {
        template <class Lit>
        struct UnicodeLiteral {
            Lit literal;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                const auto ptr = seq.rptr;
                std::uint32_t code;
                auto cvt1 = [&] {
                    if (seq.eos()) {
                        return Status::not_match;
                    }
                    auto ok = utf::to_utf32(seq, code, true);
                    if (!ok) {
                        if (ctxs::context_is_fatal_utf_error(ctx, seq, ok)) {
                            return Status::fatal;
                        }
                        seq.rptr = ptr;
                        return Status::not_match;
                    }
                    return Status::match;
                };
                if constexpr (std::is_integral_v<Lit>) {
                    if (auto s = cvt1(); s != Status::match) {
                        return s;
                    }
                    if (code != literal) {
                        seq.rptr = ptr;
                        return Status::not_match;
                    }
                }
                else {
                    auto cmps = make_ref_seq(literal);
                    while (!cmps.eos()) {
                        std::uint32_t cmp = 0;
                        if (auto ok = utf::to_utf32(cmps, cmp, true); !ok) {
                            ctxs::context_is_fatal_utf_error(ctx, seq, ok);
                            ctxs::context_error(seq, ctx, "unicode literal has invalid code point");
                            return Status::fatal;
                        }
                        if (auto s = cvt1(); s != Status::match) {
                            return s;
                        }
                        if (code != cmp) {
                            seq.rptr = ptr;
                            return Status::not_match;
                        }
                    }
                }
                return Status::match;
            }

            constexpr void must_match_error(auto&& seq, auto&& ctx, auto&& rec) const {
                ctxs::context_error(seq, ctx, "expect unicode literal ", literal, " but not");
            }
        };

        template <class Lit>
        struct UnicodeOneOfLiteral {
            Lit literal;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                const auto ptr = seq.rptr;
                std::uint32_t code;
                auto cvt1 = [&] {
                    if (seq.eos()) {
                        return Status::not_match;
                    }
                    auto ok = utf::to_utf32(seq, code, true);
                    if (!ok) {
                        if (ctxs::context_is_fatal_utf_error(ctx, seq, ok)) {
                            return Status::fatal;
                        }
                        seq.rptr = ptr;
                        return Status::not_match;
                    }
                    return Status::match;
                };
                if constexpr (std::is_integral_v<Lit>) {
                    if (auto s = cvt1(); s != Status::match) {
                        return s;
                    }
                    if (code == literal) {
                        return Status::match;
                    }
                    seq.rptr = ptr;
                }
                else {
                    auto cmps = make_ref_seq(literal);
                    while (!cmps.eos()) {
                        std::uint32_t cmp = 0;
                        if (auto ok = utf::to_utf32(cmps, cmp, true); !ok) {
                            ctxs::context_is_fatal_utf_error(ctx, seq, ok);
                            ctxs::context_error(seq, ctx, "unicode literal has invalid code point");
                            return Status::fatal;
                        }
                        if (auto s = cvt1(); s != Status::match) {
                            return s;
                        }
                        if (code == cmp) {
                            return Status::match;
                        }
                        seq.rptr = ptr;
                    }
                }
                return Status::not_match;
            }

            constexpr void must_match_error(auto&& seq, auto&& ctx, auto&& rec) const {
                ctxs::context_error(seq, ctx, "expect one of unicode ", literal, " but not");
            }
        };

        template <class LitA, class LitB>
        struct UnicodeRangeLiteral {
            LitA lita;
            LitB litb;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                if (seq.eos()) {
                    return Status::not_match;
                }
                const auto ptr = seq.rptr;
                std::uint32_t code;
                auto ok = utf::to_utf32(seq, code, true);
                if (!ok) {
                    if (ctxs::context_is_fatal_utf_error(ctx, seq, ok)) {
                        return Status::fatal;
                    }
                    seq.rptr = ptr;
                    return Status::not_match;
                }
                if (code < lita || code > litb) {
                    seq.rptr = ptr;
                    return Status::not_match;
                }
                return Status::match;
            }

            constexpr void must_match_error(auto&& seq, auto&& ctx, auto&& rec) const {
                ctxs::context_error(seq, ctx, "expect unicode range [", lita, "-", litb, "] but not");
            }
        };

        struct UnicodeAnyLiteral {
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                const auto ptr = seq.rptr;
                std::uint32_t code;
                auto ok = utf::to_utf32(seq, code, true);
                if (!ok) {
                    if (ctxs::context_is_fatal_utf_error(ctx, seq, ok)) {
                        return Status::fatal;
                    }
                    seq.rptr = ptr;
                    return Status::not_match;
                }
                return Status::match;
            }

            constexpr void must_match_error(auto&& seq, auto&& ctx, auto&& rec) const {
                ctxs::context_error(seq, ctx, "expect any unicode character but not");
            }
        };
    }  // namespace types

    namespace ops {

        template <class Lit>
        constexpr auto ulit(Lit&& lit) {
            return types::UnicodeLiteral<std::decay_t<Lit>>{std::forward<Lit>(lit)};
        }

        template <class Lit>
        constexpr auto uoneof(Lit&& lit) {
            return types::UnicodeOneOfLiteral<std::decay_t<Lit>>{std::forward<Lit>(lit)};
        }

        template <class A, class B>
        constexpr auto urange(A&& a, B&& b) {
            return types::UnicodeRangeLiteral<std::decay_t<A>, std::decay_t<B>>{std::forward<A>(a), std::forward<B>(b)};
        }

        constexpr auto uany = types::UnicodeAnyLiteral{};

        constexpr auto operator""_ul(const char* s, size_t) {
            return ulit(s);
        }

        constexpr auto operator""_ul(char s) {
            return ulit(s);
        }

        constexpr auto operator""_ul(const char8_t* s, size_t) {
            return ulit(s);
        }

        constexpr auto operator""_ul(const char16_t* s, size_t) {
            return ulit(s);
        }

        constexpr auto operator""_ul(const char32_t* s, size_t) {
            return ulit(s);
        }

        constexpr auto operator""_ul(char16_t s) {
            return ulit(s);
        }

        constexpr auto operator""_ul(char32_t s) {
            return ulit(s);
        }

        constexpr auto operator""_uoo(const char* s, size_t) {
            return uoneof(s);
        }

        constexpr auto operator""_uoo(const char8_t* s, size_t) {
            return uoneof(s);
        }

        constexpr auto operator""_uoo(const char16_t* s, size_t) {
            return uoneof(s);
        }

        constexpr auto operator""_uoo(const char32_t* s, size_t) {
            return uoneof(s);
        }

        namespace test {
            constexpr auto check_unicode() {
                auto seq = make_ref_seq(U"𠮷野家");
                auto seq2 = make_ref_seq(u"𠮷野家");
                auto lit = ulit(u8"𠮷野家");
                return lit(seq, 0, 0) == Status::match &&
                       lit(seq2, 0, 0) == Status::match;
            }

            static_assert(check_unicode());

        }  // namespace test
    }      // namespace ops

}  // namespace futils::comb2
