/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/hash_map.h"
#include "../hash_fn.h"
#include "../../storage.h"
#include "../context/context.h"

namespace utils {
    namespace fnet::quic::server {
        struct Closed {
            storage packet;
        };

        struct Server {
            slib::hash_map<flex_storage, Closed> closed_conn;
        };
    }  // namespace fnet::quic::server
}  // namespace utils
