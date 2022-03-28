/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp cnet interface
#pragma once
#include "../platform/windows/dllexport_header.h"
#include "cnet.h"

namespace utils {
    namespace cnet {
        namespace tcp {

            // create tcp client
            DLL CNet* STDCALL create_client();

            enum TCPConfig {
                // ip version
                ipver = user_defined_start + 1,
                // retry after ::connect() failed
                retry_after_connect_call,
                // use multiple io system like below
                // windows: io completion port
                // linux: epoll
                use_multiple_io_system,
                // polling recv data
                recv_polling,
                // host name
                host_name,
                // port number
                port_number,
                // raw socket (only get)
                raw_socket,
            };

            inline int ip_version(CNet* ctx) {
                return query_number(ctx, ipver);
            }

            bool set_hostport(CNet* ctx, const char* host, const char* port) {
                return set_ptr(ctx, host_name, (void*)host) && set_ptr(ctx, port_number, (void*)port);
            }

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
