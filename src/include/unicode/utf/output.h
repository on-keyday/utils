/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <concepts>
#include <cstddef>

namespace futils::unicode::internal {
    template <class T>
    concept pointer = std::is_pointer_v<T> && std::is_arithmetic_v<std::remove_pointer_t<T>>;

    template <class T, class U>
    concept has_append = requires(T t, U u) {
        { t.append(u, size_t()) };
    };

    template <class T>
    concept reftyp = std::is_reference_v<T> && (!std::is_const_v<std::remove_reference_t<T>>);

    template <class T, class U>
    concept has_string_like = requires(T t, U u) {
        { t[1] = u } -> reftyp;
        { t.size() } -> std::convertible_to<size_t>;
        { t.resize(size_t()) };
    };

    template <class T, class U>
    concept has_push_back = requires(T t, U u) {
        { t.push_back(u) };
    };

    template <class T>
    concept has_is_limit = requires(T t) {
        { t.is_limit(size_t{}) };
    };

    constexpr bool output(auto&& output, auto* d, size_t l) {
        if constexpr (has_is_limit<decltype(output)>) {
            if (output.is_limit(l)) {
                return false;
            }
        }
        if constexpr (pointer<std::decay_t<decltype(output)>>) {
            for (size_t i = 0; i < l; i++) {
                output[i] = d[i];
            }
        }
        else if constexpr (has_append<decltype(output), decltype(d)>) {
            output.append(d, l);
        }
        else if constexpr (has_string_like<decltype(output), decltype(d[1])>) {
            const auto s = output.size();
            output.resize(s + l);
            for (size_t i = 0; i < l; i++) {
                output[s + i] = d[i];
            }
        }
        else if constexpr (has_push_back<decltype(output), decltype(d[1])>) {
            for (size_t i = 0; i < l; i++) {
                output.push_back(d[i]);
            }
        }
        else {
            static_assert(has_push_back<decltype(output), decltype(d[1])>);
        }
        return true;
    }
}  // namespace futils::unicode::internal
