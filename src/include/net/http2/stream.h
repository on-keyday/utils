/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// stream - http2 stream
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "conn.h"

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {
                struct ConnectionImpl;
                struct StreamImpl;

            }  // namespace internal

            enum class Status {
                idle,
                closed,
                open,
                half_closed_remote,
            };

            struct Connection;

            struct DLL Stream {
                int id() const;
                std::uint8_t priority() const;
                std::int64_t window_size() const;
                Status status() const;

               private:
                friend struct internal::ConnectionImpl;
                friend struct Connection;
                wrap::shared_ptr<internal::StreamImpl> impl;
            };

            struct Connection {
                Stream* stream(int id);

                H2Error update_recv(const Frame& frame);

               private:
                wrap::shared_ptr<internal::ConnectionImpl> impl;
            };
        }  // namespace http2
    }      // namespace net
}  // namespace utils