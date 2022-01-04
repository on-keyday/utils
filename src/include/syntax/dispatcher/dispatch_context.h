/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dispatch_context - dispatcher context
#pragma once

#include "dispatcher.h"

namespace utils {
    namespace syntax {

        enum class FilterType {
            no_check,
            check,
        };

        template <class T>
        struct DispatchFilter {
            Dispatch<T> dispatch;
            FilterType type = FilterType::no_check;
            Filter<T> filter;
        };

        template <template <class...> class Vec>
        struct DispatchContext {
            Vec<DispatchFilter<>> disp;
        };
    }  // namespace syntax
}  // namespace utils
