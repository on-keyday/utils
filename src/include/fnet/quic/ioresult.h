/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ioresult - io result
#pragma once

namespace futils {
    namespace fnet::quic {
        enum class IOResult {
            // operation suceeded
            ok,
            // id is not match to struct holds
            id_mismatch,
            // stream state is not in specified state
            not_in_io_state,
            // no data to send exists
            no_data,
            // reach stream flow control limit
            block_by_stream,
            // reach connection flow control limit
            block_by_conn,
            // no send buffer exists
            no_capacity,
            // operation canceled
            cancel,
            // invalid data
            invalid_data,
            // fatal error
            fatal,
        };

        constexpr bool ioresult_ok(IOResult res) noexcept {
            return res == IOResult::ok;
        }

        constexpr const char* to_string(IOResult res) noexcept {
            switch (res) {
                case IOResult::ok:
                    return nullptr;
                case IOResult::id_mismatch:
                    return "internal id mismatch error. library bug!!";
                case IOResult::not_in_io_state:
                    return "stream state is not in IO";
                case IOResult::no_data:
                    return "no data available";
                case IOResult::block_by_conn:
                    return "connection blocked by flow control limit";
                case IOResult::block_by_stream:
                    return "stream blocked by flow control limit";
                case IOResult::no_capacity:
                    return "no capacity to write";
                case IOResult::cancel:
                    return "operation canceled";
                case IOResult::invalid_data:
                    return "invalid input data. library bug!!";
                case IOResult::fatal:
                    return "fatal error library bug!!";
            }
        }
    }  // namespace fnet::quic
}  // namespace futils
