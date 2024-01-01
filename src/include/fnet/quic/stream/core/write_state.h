/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include "../../ioresult.h"

namespace futils::fnet::quic::stream::core {
    struct StreamWriteBufferState {
        IOResult result;
        size_t perfect_written_buffer_count = 0;
        size_t written_size_of_last_buffer = 0;
    };
}  // namespace futils::fnet::quic::stream::core
