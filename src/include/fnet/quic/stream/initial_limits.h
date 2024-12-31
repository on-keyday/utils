/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// initial_limits
#pragma once
#include <cstdint>

namespace futils {
    namespace fnet::quic::stream {
        struct InitialLimits {
            std::uint64_t conn_data_limit = 0;
            std::uint64_t bidi_stream_limit = 0;
            std::uint64_t uni_stream_limit = 0;
            std::uint64_t bidi_stream_data_local_limit = 0;
            std::uint64_t bidi_stream_data_remote_limit = 0;
            std::uint64_t uni_stream_data_limit = 0;
        };

    }  // namespace fnet::quic::stream
}  // namespace futils
