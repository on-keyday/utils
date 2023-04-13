/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../internal/opti.h"
#include "../internal/context_fn.h"
#include "../status.h"
#include "../../core/sequencer.h"

namespace utils::comb2 {
    namespace types {
        template <class Tag, class A>
        struct Group : opti::MaybeEmptyA<A> {
            Tag tag;
            constexpr Group(auto&& tag_, auto&& fn_)
                : tag(std::forward<decltype(tag_)>(tag_)),
                  opti::MaybeEmptyA<A>(std::forward<decltype(fn_)>(fn_)) {}

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx& ctx, Rec&& r) const {
                ctxs::context_group(ctx, tag);
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
                }
                return res;
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
                const Status res = this->useA()(seq, ctx, r);
                if (res == Status::fatal) {
                    return res;
                }
                const auto end = seq.rptr;
                ctxs::context_end_string(ctx, tag, seq, Pos{begin, end});
                return res;
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

}  // namespace utils::comb2
