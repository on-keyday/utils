/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn - quic connection context
#pragma once
#include "../core/core.h"

namespace utils {
    namespace quic {
        namespace conn {
            struct Connection;
            Connection* new_connection(core::QUIC* q);
        }  // namespace conn
    }      // namespace quic
}  // namespace utils
