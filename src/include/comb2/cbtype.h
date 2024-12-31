/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// cbtype - callback type
#pragma once

namespace futils {
    namespace comb2 {
        enum class CallbackType {
            optional_entry,
            optional_result,
            branch_entry,
            branch_other,
            branch_result,
            repeat_entry,
            repeat_step,
            repeat_result,
            peek_begin,
            peek_end,
        };
    }
}  // namespace futils
