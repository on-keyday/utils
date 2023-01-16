/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn - quic connection context
#pragma once
#include "../common/dll_h.h"
#include "../core/core.h"
#include "../mem/shared_ptr.h"

namespace utils {
    namespace quic {
        namespace conn {
            struct Connection;
            Dll(Connection*) new_connection(core::QUIC* q, Mode mode);
            using Conn = mem::shared_ptr<Connection>;
        }  // namespace conn
    }      // namespace quic
}  // namespace utils
