/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// filter - predefined filter
#pragma once
#include "../../helper/strutil.h"

namespace utils {
    namespace syntax {
        namespace filter {

            namespace internal {
                template <bool incr, class T, class... Arg>
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
                        return stack_eq<incr>(index + 1, ctx, std::forward<Args>(args)...);
                    }
                    return stack_eq<incr>(index + 1, ctx, std::forward<Args>(args)...);
                }

            }  // namespace internal

            template <class... Args>
            auto stack(size_t first_index, Args&&... args) {
                return [=](auto& ctx) {
                    return internal::stack_eq(false, first_index, ctx, std::forward<Args>(args)...);
                };
            }

        }  // namespace filter

    }  // namespace syntax
}  // namespace utils
