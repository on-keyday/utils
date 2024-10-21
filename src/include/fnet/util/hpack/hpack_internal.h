/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <concepts>

namespace futils::hpack::internal {
    template <class String, class From>
    concept convertible_to_string = requires(From s) {
        { String(s) } -> std::convertible_to<String>;
    };

    template <class String>
    constexpr String convert_string(auto&& s) {
        if constexpr (std::is_same_v<std::decay_t<decltype(s)>, String>) {
            return s;
        }
        else if constexpr (convertible_to_string<String, decltype(s)>) {
            return String(s);
        }
        else {
            auto seq = make_ref_seq(s);
            String buf;
            while (!seq.eos()) {
                buf.push_back(seq.current());
                seq.consume();
            }
            return buf;
        }
    }
}  // namespace futils::hpack::internal