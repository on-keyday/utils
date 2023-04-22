/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/hash_map.h"
#include "../hash_fn.h"

namespace utils {
    namespace fnet::quic::server {
        struct Closed {};

        struct Server {
            slib::hash_map<view::rvec, Closed> closed_conn;
        };
    }  // namespace fnet::quic::server
}  // namespace utils
