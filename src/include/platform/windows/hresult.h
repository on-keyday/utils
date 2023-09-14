/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/flags.h>

namespace utils::platform::windows {
    struct HResult {
       private:
        binary::flags_t<std::uint32_t, 1, 1, 1, 1, 1, 11, 16> val;

       public:
        bits_flag_alias_method(val, 0, failure);

        constexpr bool success() const noexcept {
            return !failure();
        }

        constexpr void set_success(bool v) noexcept {
            return set_failure(!v);
        }

        bits_flag_alias_method(val, 5, facility);
        bits_flag_alias_method(val, 6, code);

        constexpr HResult() = default;
        constexpr HResult(std::uint32_t v) {
            val.as_value() = v;
        }
        constexpr operator std::uint32_t() const noexcept {
            return val.as_value();
        }
    };
}  // namespace utils::platform::windows
