/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>

namespace futils::fnet::http1 {
    struct Range {
        size_t start = 0;
        size_t end = 0;
    };

    struct FieldRange {
        Range key;
        Range value;
    };
}  // namespace futils::fnet::http1