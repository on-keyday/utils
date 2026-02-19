/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../internal/opti.h"
#include "../internal/context_fn.h"
#include "../status.h"
#include "../../core/sequencer.h"

namespace futils::comb2 {
    namespace types {
        template <class Tag, class A>
        struct Group : opti::MaybeEmptyA<A> {
            Tag tag;
            constexpr Group(auto&& tag_, auto&& fn_)
                : tag(std::forward<decltype(tag_)>(tag_)),
                  opti::MaybeEmptyA<A>(std::forward<decltype(fn_)>(fn_)) {}

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
                ctxs::context_begin_group(ctx, tag);
                const auto begin = seq.rptr;
                const Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                const auto end = seq.rptr;
                ctxs::context_end_group(ctx, res, tag, Pos{.begin = begin, .end = end});
                return res;
            }

            constexpr void must_match_error(auto&& seq, auto&& ctx, auto&& rec) const {
                ctxs::context_error(seq, ctx, "not match to group. tag: ", tag);
                ctxs::context_call_must_match_error(seq, ctx, this->useA(), rec);
            }
        };

        template <class Tag, class A>
        struct String : opti::MaybeEmptyA<A> {
            Tag tag;

            constexpr String(auto&& tag_, auto&& fn_)
                : tag(std::forward<decltype(tag_)>(tag_)),
                  opti::MaybeEmptyA<A>(std::forward<decltype(fn_)>(fn_)) {}

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
                const auto begin = seq.rptr;
                ctxs::context_begin_string(ctx, tag);
                Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                const auto end = seq.rptr;
                // user may change res so it may terminates parsing
                ctxs::context_end_string(ctx, res, tag, seq, Pos{begin, end});
                return res;
            }

            constexpr void must_match_error(auto&& seq, auto&& ctx, auto&& rec) const {
                ctxs::context_error(seq, ctx, "not match to string. tag: ", tag);
                ctxs::context_call_must_match_error(seq, ctx, this->useA(), rec);
            }
        };
    }  // namespace types

    namespace ops {

        template <class Tag, class A>
        constexpr auto group(Tag&& tag, A&& a) {
            return types::Group<std::decay_t<Tag>, std::decay_t<A>>{
                std::forward<Tag>(tag),
                std::forward<A>(a),
            };
        }

        template <class Tag, class A>
        constexpr auto str(Tag&& tag, A&& a) {
            return types::String<std::decay_t<Tag>, std::decay_t<A>>{
                std::forward<Tag>(tag),
                std::forward<A>(a),
            };
        }
    }  // namespace ops

}  // namespace futils::comb2
