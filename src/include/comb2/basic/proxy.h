/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../internal/opti.h"
#include "../../core/sequencer.h"
#include "../status.h"

namespace utils::comb2 {
    namespace types {

        template <class A>
        struct Proxy : opti::MaybeEmptyA<A> {
            using opti::MaybeEmptyA<A>::MaybeEmptyA;

            template <class T, class Ctx, class Rec>
            constexpr Status operator()(Sequencer<T>& seq, Ctx&& ctx, Rec&& r) const {
                return this->useA()(seq, ctx, r);
            }
        };

    }  // namespace types

    namespace ops {
        template <class Fn>
        constexpr auto proxy(Fn&& fn) {
            return types::Proxy<std::decay_t<Fn>>{
                std::forward<Fn>(fn),
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

#define method_proxy(method) proxy([](auto&& seq, auto&& ctx, auto&& rec) -> Status { return rec.method(seq, ctx, rec); })
    }  // namespace ops
}  // namespace utils::comb2
