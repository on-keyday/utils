/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>

namespace utils {
    namespace fnet::quic::datagram {

        struct Config {
            size_t pending_limit = 3;
            size_t recv_buf_limit = ~0;
        };

    }  // namespace fnet::quic::datagram
}  // namespace utils