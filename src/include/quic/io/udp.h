/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// udp - wrapper of user datagram protocol
#pragma once
#include "../internal/alloc.h"
#include "io.h"

namespace utils {
    namespace quic {
        namespace io {
            namespace udp {
                io::IO Protocol(allocate::Alloc* a);

                constexpr TargetKey UDP_IPv4 = 17;
                constexpr TargetKey UDP_IPv6 = 18;
            }  // namespace udp
        }      // namespace io
    }          // namespace quic
}  // namespace utils