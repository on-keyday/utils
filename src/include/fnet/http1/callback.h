/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <utility>
#include <concepts>
#include "error_enum.h"
#include "range.h"

namespace futils::fnet::http1 {
    template <class F, class... A>
    constexpr header::HeaderErr call_and_convert_to_HeaderError(F&& f, A&&... a) {
        if constexpr (std::is_convertible_v<std::invoke_result_t<F, A...>, header::HeaderError>) {
            return f(std::forward<A>(a)...);
        }
        else if constexpr (std::is_convertible_v<std::invoke_result_t<F, A...>, bool>) {
            if (!f(std::forward<A>(a)...)) {
                return header::HeaderError::validation_error;
            }
            return header::HeaderError::none;
        }
        else {
            f(std::forward<A>(a)...);
            return header::HeaderError::none;
        }
    }

    template <class H, class K, class V>
    constexpr header::HeaderError apply_call_or_emplace(H&& h, K&& key, V&& value) {
        if constexpr (std::invocable<decltype(h), decltype(std::move(key)), decltype(std::move(value))>) {
            return call_and_convert_to_HeaderError(std::forward<H>(h), std::forward<K>(key), std::forward<V>(value));
        }
        else {
            h.emplace(std::move(key), std::move(value));
            return header::HeaderError::none;
        }
    }

    template <class String, class F>
    constexpr auto field_range_to_string(F&& f) {
        return [=](auto& seq, FieldRange range) {
            String key{}, value{};
            range_to_string(seq, key, range.key);
            range_to_string(seq, value, range.value);
            return apply_call_or_emplace(f, std::move(key), std::move(value));
        };
    }

    template <class H, class F>
    constexpr header::HeaderErr apply_call_or_iter(H&& h, F&& f) {
        if constexpr (std::invocable<decltype(h), decltype(std::move(f))>) {
            return call_and_convert_to_HeaderError(std::forward<H>(h), std::forward<F>(f));
        }
        else {
            for (auto&& kv : h) {
                if (auto res = call_and_convert_to_HeaderError(std::forward<F>(f), get<0>(kv), get<1>(kv)); !res) {
                    return res;
                }
            }
            return header::HeaderError::none;
        }
    }

    template <class String>
    auto default_header_callback(auto&& header) {
        return field_range_to_string<String>([&](auto&& key, auto&& value) {
            return apply_call_or_emplace(header, std::move(key), std::move(value));
        });
    }
}  // namespace futils::fnet::http1
