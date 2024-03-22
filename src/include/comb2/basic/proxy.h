/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../internal/opti.h"
#include "../../core/sequencer.h"
#include "../status.h"
#include "../internal/context_fn.h"

namespace futils::comb2 {
    namespace types {

        struct MustMatchErrorFn {
            constexpr void operator()(auto&& seq, auto&& ctx, auto&& rec) const {}
        };

        template <class A, class B>
        struct Proxy : opti::MaybeEmptyAB<A, B> {
            using opti::MaybeEmptyAB<A, B>::MaybeEmptyAB;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                return this->useA()(seq, ctx, r);
            }

            constexpr auto must_match_error(auto&& seq, auto&& ctx, auto&& rec) const {
                this->useB()(seq, ctx, rec);
            }
        };

    }  // namespace types

    namespace ops {
        template <class Fn, class MustMatchError = types::MustMatchErrorFn>
        constexpr auto proxy(Fn&& fn, MustMatchError&& err = MustMatchError()) {
            return types::Proxy<std::decay_t<Fn>, std::decay_t<MustMatchError>>{
                std::forward<Fn>(fn),
                std::forward<MustMatchError>(err),
            };
        }
        /*
        #define direct_proxy(...) []() {                                              \
            struct L {                                                                \
                constexpr auto operator()(auto&& seq, auto&& ctx, auto&& rec) const { \
                    return __VA_ARGS__(seq, ctx, rec);                                \
                }                                                                     \
            };                                                                        \
            return proxy(L{});                                                        \
        }()
        */

#define method_proxy(method)                                                                                       \
    proxy([](auto&& seq, auto&& ctx, auto&& rec) -> ::futils::comb2::Status { return rec.method(seq, ctx, rec); }, \
          [](auto&& seq, auto&& ctx, auto&& rec) { ::futils::comb2::ctxs::context_call_must_match_error(seq, ctx, rec.method, rec); })

#define decl_method_proxy(method) decltype(method##_) method

#define conditional_method(cond, default_status, method)                         \
    proxy([](auto&& seq, auto&& ctx, auto&& rec) -> ::futils::comb2::Status { if(!rec.cond) { return default_status; }  return method(seq, ctx, rec); }, \
          [](auto&& seq, auto&& ctx, auto&& rec) { ::futils::comb2::ctxs::context_call_must_match_error(seq, ctx, method, rec); })
    }  // namespace ops
}  // namespace futils::comb2
