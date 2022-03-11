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
#include "../http/http1.h"

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
                half_closed_local,
                reserved_remote,
                reserved_local,
            };

            enum class StreamError {
                none,
                invalid_status,
                setting_was_ignored,
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
                Stream* new_stream();

                H2Error update_recv(const Frame& frame);
                H2Error update_send(const Frame& frame);

                bool make_data(std::int32_t id, wrap::string& data, DataFrame& frame,bool& block);
                bool make_header(http::Header&& h, HeaderFrame& frame, wrap::string& remain);
                bool make_continuous(std::int32_t id, wrap::string& remain, Continuation& frame);

               private:
                wrap::shared_ptr<internal::ConnectionImpl> impl;
            };
        }  // namespace http2
    }      // namespace net
}  // namespace utils
