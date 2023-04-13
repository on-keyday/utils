/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../quic/varint.h"

namespace utils {
    namespace fnet::http3 {
        struct Frame {
            quic::varint::Value type;
        };
    }  // namespace fnet::http3
}  // namespace utils
