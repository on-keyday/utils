/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <utility>

namespace futils::helper {

    template <class T>
    struct omit_empty {
        T value;
        constexpr omit_empty() = default;
        constexpr omit_empty(auto&& value)
            : value(std::forward<decltype(value)>(value)) {}

        constexpr const T& om_value() const noexcept {
            return value;
        }
        constexpr T& om_value() noexcept {
            return value;
        }

        constexpr void move_om_value(T&& value) noexcept {
            this->value = std::move(value);
        }

        constexpr void copy_om_value(const T& value) {
            this->value = value;
        }
    };

    template <class T>
        requires std::is_empty_v<T>
    struct omit_empty<T> {
        constexpr omit_empty() = default;
        constexpr omit_empty(auto&&) {}

        constexpr T om_value() const noexcept {
            return T{};
        }

        constexpr void move_om_value(T&&) noexcept {}
        constexpr void copy_om_value(const T&) noexcept {}
    };
}  // namespace futils::helper
