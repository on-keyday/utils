/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <wrap/light/enum.h>

namespace futils::hpack {
    enum class HpackError {
        none,
        too_large_number,
        too_short_number,
        internal,
        input_length,
        output_length,
        invalid_mask,
        invalid_value,
        not_exists,
        table_update,
    };

    using HpkErr = wrap::EnumWrap<HpackError, HpackError::none, HpackError::internal>;

    constexpr const char* to_string(HpackError e) {
        switch (e) {
            case HpackError::none:
                return "no error";
            case HpackError::too_large_number:
                return "too large number";
            case HpackError::too_short_number:
                return "too short number";
            case HpackError::internal:
                return "internal error";
            case HpackError::input_length:
                return "input length error";
            case HpackError::output_length:
                return "output length error";
            case HpackError::invalid_mask:
                return "invalid mask";
            case HpackError::invalid_value:
                return "invalid value";
            case HpackError::not_exists:
                return "not exists";
            case HpackError::table_update:
                return "table update error";
        }
        return "unknown or internal error";
    }

}  // namespace futils::hpack
