/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// filter - predefined filter
#pragma once
#include "../../helper/equal.h"

namespace utils {
    namespace syntax {
        namespace filter {

            namespace internal {
                template <bool incr, class T, class... Args>
                bool stack_eq(size_t index, T& ctx, Args&&...) {
                    return true;
                }

                template <bool incr, class T, class C, class... Args>
                bool stack_eq(size_t index, T& ctx, C&& current, Args&&... args) {
                    if (ctx.stack.size() <= index) {
                        return false;
                    }
                    if (!helper::equal(ctx.stack[ctx.stack.size() - 1 - index], current)) {
                        if constexpr (!incr) {
                            return false;
                        }
                        else {
                            return stack_eq<incr>(index + 1, ctx, std::forward<C>(current), std::forward<Args>(args)...);
                        }
                    }
                    return stack_eq<incr>(index + 1, ctx, std::forward<Args>(args)...);
                }

            }  // namespace internal

            template <class... Args>
            auto stack_strict(size_t first_index, Args&&... args) {
                return [=](auto& ctx) {
                    return internal::stack_eq<false>(first_index, ctx, std::forward<Args>(args)...);
                };
            }

            template <class... Args>
            auto stack_order(size_t first_index, Args&&... args) {
                return [=](auto& ctx) {
                    return internal::stack_eq<true>(first_index, ctx, std::forward<Args>(args)...);
                };
            }

            template <class A, class B>
            auto and_filter(A&& a, B&& b) {
                return [=](auto& ctx) {
                    return a(ctx) && b(ctx);
                };
            }

            template <class A, class B>
            auto or_filter(A&& a, B&& b) {
                return [=](auto& ctx) {
                    return a(ctx) || b(ctx);
                };
            }

            template <class A>
            auto not_filter(A&& a) {
                return [=](auto& ctx) {
                    return !a(ctx);
                };
            }

        }  // namespace filter

    }  // namespace syntax
}  // namespace utils
