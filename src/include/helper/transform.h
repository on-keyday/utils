/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>

namespace futils::helper::either {
    constexpr auto push_back_to(auto& vec) {
        return [&](auto&& v) {
            vec.push_back(std::move(v));
        };
    }

    constexpr auto assign_to(auto& val) {
        return [&](auto&& v) {
            val = std::move(v);
        };
    }

    template <class T>
    constexpr auto cast_to(auto& val) {
        return [&](auto&& v) {
            val = T(std::move(v));
        };
    }

    template <class T>
    constexpr auto empty_value() {
        return [](auto&&...) {
            if constexpr (!std::is_void_v<T>) {
                return T{};
            }
        };
    }

}  // namespace futils::helper::either