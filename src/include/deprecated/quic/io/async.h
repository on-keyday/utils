/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// async - async io helper
#pragma once
#include "../common/dll_h.h"

namespace utils {
    namespace quic {

        namespace core {
            struct QUIC;
        }

        namespace io {
            using AsyncHandle = void*;

            io::AsyncHandle get_async_handle(core::QUIC* q);
            Dll(io::Result) register_target(core::QUIC* q, io::TargetID id);
        }  // namespace io
    }      // namespace quic
}  // namespace utils
