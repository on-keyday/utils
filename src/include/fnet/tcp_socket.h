/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "socket.h"

namespace utils {
    namespace fnet::tcp {

        struct TCPSocket {
           private:
            Socket sock;
        };
    }  // namespace fnet::tcp
}  // namespace utils