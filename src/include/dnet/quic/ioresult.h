/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ioresult - io result
#pragma once

namespace utils {
    namespace dnet::quic {
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
    }
}  // namespace utils
