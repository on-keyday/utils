/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ioresult - io result
#pragma once

namespace utils {
    namespace dnet::quic {
        enum class IOResult {
            ok,
            id_mismatch,
            not_in_io_state,
            no_data,
            block_by_stream,
            block_by_conn,
            no_capacity,
            cancel,
            invalid_data,
            fatal,
        };
    }
}  // namespace utils
