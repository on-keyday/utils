/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// context - libquic contexts
#pragma once

namespace utils {
    namespace quic {
        namespace context {
            // QUIC has a number of Connection objects
            struct QUIC;
            // Connection has a number of Stream objects and a IO object
            // Connection has 2 mode, client and server
            struct Connection;
            // Stream manages stream state and stream flow control
            struct Stream;
            // IO manages low level io components.
            // IO is shared among Connection if IO supports it.
            // IO supports not only single endpoint but
            // multiple endpoints.
            struct IO;
        }  // namespace context
    }      // namespace quic
}  // namespace utils
