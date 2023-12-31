/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <string>
#include <comb2/pos.h>

namespace qurl {
    struct Error {
        std::string msg;
        utils::comb2::Pos pos;
    };
}  // namespace qurl
