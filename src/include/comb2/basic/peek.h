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
                ctxs::context_logic_entry(ctx, CallbackType::peek_begin);
                const auto ptr = seq.rptr;
                Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    ctxs::context_error(ctx, "Status::fatal at peeking");
                    return res;
                }
                seq.rptr = ptr;
                ctxs::context_logic_result(ctx, CallbackType::peek_end, res);
                return res;
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_error(ctx, "peek and expect so but fail");
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
            }
        };

        template <class A>
        struct Not : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                ctxs::context_logic_entry(ctx, CallbackType::peek_begin);
                const auto ptr = seq.rptr;
                Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    ctxs::context_error(ctx, "Status::fatal at peeking");
                    return res;
                }
                seq.rptr = ptr;
                ctxs::context_logic_result(ctx, CallbackType::peek_end, res);
                return res == Status::match ? Status::not_match : Status::match;
            }

            constexpr void must_match_error(auto&& ctx, auto&& rec) const {
                ctxs::context_error(ctx, "peek and expect NOT so but fail");
                ctxs::context_call_must_match_error(ctx, this->useA(), rec);
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
