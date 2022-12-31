/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../socket.h"
#include "../addrinfo.h"
namespace utils {
    namespace dnet {
        namespace server {
            struct Client {
                Socket sock;
                NetAddrPort addr;
            };
        }  // namespace server
    }      // namespace dnet
}  // namespace utils
