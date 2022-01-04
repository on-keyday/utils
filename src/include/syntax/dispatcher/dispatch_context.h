/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dispatch_context - dispatcher context
#pragma once

#include "dispatcher.h"
#include "../../wrap/lite/enum.h"
#include "../matching/context.h"

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
            MatchState operator()(context_t& ctx) {
                for (DispatchFilter<context_t>& filter : disp) {
                    if (any(filter.type & FilterType::filter)) {
                        if (!filter.filter(ctx)) {
                            continue;
                        }
                    }
                    auto res = filter.dispatch(ctx);
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
