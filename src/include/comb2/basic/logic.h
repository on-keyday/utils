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
        };

        template <class A, class B>
        struct Or : opti::MaybeEmptyAB<A, B> {
            using opti::MaybeEmptyAB<A, B>::MaybeEmptyAB;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_branch(ctx);
                const auto ptr = seq.rptr;
                Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                if (res == Status::match) {
                    ctxs::context_commit(ctx);
                    return Status::match;
                }
                ctxs::context_rollback(ctx);
                ctxs::context_branch(ctx);
                seq.rptr = ptr;
                const auto res2 = this->useB()(seq, ctx, r);
                if (res2 == Status::fatal) {
                    return res2;
                }
                if (res2 == Status::match) {
                    ctxs::context_commit(ctx);
                    return Status::match;
                }
                ctxs::context_rollback(ctx);
                return Status::not_match;
            }
        };

        template <class A>
        struct Optional : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_optional(ctx);
                const auto ptr = seq.rptr;
                const Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                if (res == Status::match) {
                    ctxs::context_commit(ctx);
                }
                else {
                    ctxs::context_rollback(ctx);
                    seq.rptr = ptr;
                }
                return Status::match;
            }
        };

        template <class A>
        struct Repeat : opti::MaybeEmptyA<A> {
            static_assert(!test::is_template_instance_of<A, Optional>, "`~-` is not allowed. use `-~` instead");
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_repeat(ctx);
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
                    ctxs::context_step(ctx);
                    first = false;
                }
                if (first) {
                    ctxs::context_rollback(ctx);
                }
                else {
                    ctxs::context_commit(ctx);
                }
                return first ? Status::not_match : Status::match;
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
                ctxs::context_repeat(ctx);
                size_t i = 0;
                for (; i < max_; i++) {
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
                    ctxs::context_step(ctx);
                }
                if (i < min_) {
                    ctxs::context_rollback(ctx);
                }
                else {
                    ctxs::context_commit(ctx);
                }
                return i < min_ ? Status::not_match : Status::match;
            }
        };

        template <class A>
        struct MustMatch : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;
            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                const Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                if (res == Status::not_match) {
                    ctxs::context_error(ctx, "not matched on must match");
                    return Status::fatal;
                }
                return Status::match;
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
