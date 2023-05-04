/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../status.h"
#include "../internal/test.h"
#include "../internal/opti.h"
#include "../internal/context_fn.h"
#include "../../core/sequencer.h"
#include "../cbtype.h"

namespace utils::comb2 {
    namespace types {

        template <class A, class B>
        struct And : opti::MaybeEmptyAB<A, B> {
            using opti::MaybeEmptyAB<A, B>::MaybeEmptyAB;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                const auto res = this->useA()(seq, ctx, r);
                if (res != Status::match) {
                    return res;
                }
                return this->useB()(seq, ctx, r);
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
                ctxs::context_call_must_match_error(ctx, this->useB(), rec);
            }
        };

        template <class A, class B>
        struct Or : opti::MaybeEmptyAB<A, B> {
            using opti::MaybeEmptyAB<A, B>::MaybeEmptyAB;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_logic_entry(ctx, CallbackType::branch_entry);
                const auto ptr = seq.rptr;
                Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                if (res == Status::match) {
                    ctxs::context_logic_result(ctx, CallbackType::branch_result, Status::match);
                    return Status::match;
                }
                ctxs::context_logic_result(ctx, CallbackType::branch_other, Status::not_match);
                ctxs::context_logic_entry(ctx, CallbackType::branch_other);
                seq.rptr = ptr;
                const auto res2 = this->useB()(seq, ctx, r);
                if (res2 == Status::fatal) {
                    return res2;
                }
                if (res2 == Status::match) {
                    ctxs::context_logic_result(ctx, CallbackType::branch_result, Status::match);
                    return Status::match;
                }
                ctxs::context_logic_result(ctx, CallbackType::branch_result, Status::not_match);
                return Status::not_match;
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
                ctxs::context_call_must_match_error(ctx, this->useB(), rec);
            }
        };

        template <class A>
        struct Optional : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_logic_entry(ctx, CallbackType::optional_entry);
                const auto ptr = seq.rptr;
                const Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                ctxs::context_logic_result(ctx, CallbackType::optional_result, res);
                if (res == Status::not_match) {
                    seq.rptr = ptr;
                }
                return Status::match;
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
            }
        };

        template <class A>
        struct Repeat : opti::MaybeEmptyA<A> {
            static_assert(!test::is_template_instance_of<A, Optional>, "`~-` is not allowed. use `-~` instead");
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_logic_entry(ctx, CallbackType::repeat_entry);
                bool first = true;
                for (;;) {
                    const auto ptr = seq.rptr;
                    const Status res = this->useA()(seq, ctx, r);
                    if (res == Status::fatal) {
                        return res;
                    }
                    if (res == Status::not_match) {
                        seq.rptr = ptr;
                        break;
                    }
                    // detect infinity loop model
                    if (seq.rptr <= ptr) {
                        ctxs::context_error(ctx, "detect infinity loop at ", seq.rptr);
                        return Status::fatal;
                    }
                    ctxs::context_logic_result(ctx, CallbackType::repeat_step, Status::match);
                    ctxs::context_logic_entry(ctx, CallbackType::repeat_step);
                    first = false;
                }
                ctxs::context_logic_result(ctx, CallbackType::repeat_result, first ? Status::not_match : Status::match);
                return first ? Status::not_match : Status::match;
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
            }
        };

        template <class Min, class Max, class A>
        struct LimitedRepeat : opti::MaybeEmptyA<A> {
            static_assert(!test::is_template_instance_of<A, Optional>, "`~-` is not allowed. use `-~` instead");
            Min min_;
            Max max_;

            constexpr LimitedRepeat(auto&& max_, auto&& min_, auto&& fn_)
                : min_(std::forward<decltype(min_)>(min_)),
                  max_(std::forward<decltype(max_)>(max_)),
                  opti::MaybeEmptyA<A>(std::forward<decltype(fn_)>(fn_)) {}

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_logic_entry(ctx, CallbackType::repeat_entry);
                size_t i = 0;
                for (; i < max_; i++) {
                    const auto ptr = seq.rptr;
                    const Status res = this->useA()(seq, ctx, r);
                    if (res == Status::fatal) {
                        return res;
                    }
                    if (res == Status::not_match) {
                        ctxs::context_logic_result(ctx, CallbackType::repeat_step, Status::not_match);
                        seq.rptr = ptr;
                        break;
                    }
                    // detect infinity loop model
                    if (seq.rptr <= ptr) {
                        ctxs::context_error(ctx, "detect infinity loop at ", seq.rptr);
                        return Status::fatal;
                    }
                    ctxs::context_logic_entry(ctx, CallbackType::repeat_step);
                }
                ctxs::context_logic_result(ctx, CallbackType::repeat_result, i < min_ ? Status::not_match : Status::match);
                return i < min_ ? Status::not_match : Status::match;
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
            }
        };

        template <class A>
        struct MustMatch : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& rec) const {
                const Status res = this->useA()(seq, ctx, rec);
                if (res == Status::fatal) {
                    return res;
                }
                if (res == Status::not_match) {
                    ctxs::context_call_must_match_error(ctx, this->useA(), rec);
                    return Status::fatal;
                }
                return Status::match;
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
            }
        };
    }  // namespace types

    namespace ops {

        template <class A, class B>
        constexpr auto operator&(A&& a, B&& b) {
            return types::And<std::decay_t<A>, std::decay_t<B>>{
                std::forward<A>(a),
                std::forward<B>(b),
            };
        }

        template <class A, class B>
        constexpr auto operator|(A&& a, B&& b) {
            return types::Or<std::decay_t<A>, std::decay_t<B>>{
                std::forward<A>(a),
                std::forward<B>(b),
            };
        }
        // optional (weaker)
        template <class A>
        constexpr auto operator-(A&& a) {
            return types::Optional<std::decay_t<A>>{
                std::forward<A>(a),
            };
        }

        // repeat
        template <class A>
        constexpr auto operator~(A&& a) {
            return types::Repeat<std::decay_t<A>>{
                std::forward<A>(a),
            };
        }

        // must match (stronger)
        template <class A>
        constexpr auto operator+(A&& a) {
            return types::MustMatch<std::decay_t<A>>{
                std::forward<A>(a),
            };
        }

        template <class Min, class Max, class A>
        constexpr auto limrep(Min&& mn, Max&& mx, A&& a) {
            return types::LimitedRepeat<std::decay_t<Min>, std::decay_t<Max>, std::decay_t<A>>{
                std::forward<Min>(mn),
                std::forward<Max>(mx),
                std::forward<A>(a),
            };
        }
    }  // namespace ops
}  // namespace utils::comb2
