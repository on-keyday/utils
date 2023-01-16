/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dispatch_context - dispatcher context
#pragma once

#include "dispatcher.h"
#include <wrap/light/enum.h>
#include "../matching/context.h"
#include <helper/compare_type.h>

namespace utils {
    namespace syntax {

        enum class FilterType {
            none = 0,
            filter = 0x1,
            check = 0x2,
            final = 0x4,
        };

        DEFINE_ENUM_FLAGOP(FilterType)

        template <class T>
        struct DispatchFilter {
            Dispatch<T> dispatch;
            FilterType type = FilterType::none;
            Filter<T> filter;
        };

        template <class String, template <class...> class Vec>
        struct DispatchContext {
            using context_t = MatchContext<String, Vec>;
            Vec<DispatchFilter<context_t>> disp;
            size_t prev = 0;

            template <class F, class B = decltype(helper::no_check())>
            DispatchContext& append(F&& f, FilterType type = FilterType::filter, B&& filter = helper::no_check()) {
                disp.push_back(
                    DispatchFilter<context_t>{
                        .dispatch = std::forward<F>(f),
                        .type = type,
                        .filter = std::forward<B>(filter),
                    });
                return *this;
            }

            MatchState operator()(context_t& ctx) {
                for (DispatchFilter<context_t>& filter : disp) {
                    if (any(filter.type & FilterType::filter)) {
                        if (!filter.filter(ctx)) {
                            continue;
                        }
                    }
                    auto res = filter.dispatch(ctx, false);
                    if (any(filter.type & FilterType::check)) {
                        if (res != MatchState::not_match) {
                            return res;
                        }
                    }
                    if (any(filter.type & FilterType::final)) {
                        return res;
                    }
                }
                return MatchState::succeed;
            }
        };
    }  // namespace syntax
}  // namespace utils
