/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/number.h>

namespace utils::fnet::ntp {
    struct NTPShort {
        std::uint16_t flags;
    };
}  // namespace utils::fnet::ntp