/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// scoped - scoped value
#pragma once
#include <type_traits>
#include "defer.h"

namespace futils::helper {
    template <class T>
    struct Scoped {
       private:
        T value;

       public:
        template <class A>
        constexpr Scoped(A&& value) : value(std::forward<A>(value)) {}

        constexpr Scoped(const Scoped&) = delete;
        constexpr Scoped& operator=(const Scoped&) = delete;

        [[nodiscard]] constexpr auto set(std::decay_t<T> new_value) {
            auto old_value = std::move(value);
            value = std::move(new_value);
            return defer([this, old_value = std::move(old_value)] {
                value = std::move(old_value);
            });
        }

        constexpr decltype(auto) operator()() const {
            const auto& v = value;
            return v;
        }
    };
}  // namespace futils::helper