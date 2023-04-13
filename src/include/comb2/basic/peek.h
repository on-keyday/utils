/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../internal/opti.h"
#include "../status.h"
#include "../internal/context_fn.h"
#include "../../core/sequencer.h"

namespace utils::comb2 {
    namespace types {

        template <class A>
        struct Peek : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_peek(ctx);
                const auto ptr = seq.rptr;
                Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    ctxs::context_error(ctx, "Status::fatal at peeking");
                    return res;
                }
                seq.rptr = ptr;
                ctxs::context_rollback(ctx);
                return res;
            }
        };

        template <class A>
        struct Not : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_peek(ctx);
                const auto ptr = seq.rptr;
                Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    ctxs::context_error(ctx, "Status::fatal at peeking");
                    return res;
                }
                seq.rptr = ptr;
                ctxs::context_rollback(ctx);
                return res == Status::match ? Status::not_match : Status::match;
            }
        };
    }  // namespace types

    namespace ops {

        template <class A>
        constexpr auto peek(A&& a) {
            return types::Peek<std::decay_t<A>>{std::forward<A>(a)};
        }

        template <class A>
        constexpr auto not_(A&& a) {
            return types::Not<std::decay_t<A>>{std::forward<A>(a)};
        }
    }  // namespace ops
}  // namespace utils::comb2
