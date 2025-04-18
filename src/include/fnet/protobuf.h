/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// protobuf - protocol buffers format
#pragma once
#include <cstddef>

namespace futils {
    namespace fnet {
        namespace protobuf {
            enum class WireType {
                varint = 0,
                i64 = 1,
                len = 2,
                sgroup = 3,  // deprecated
                egroup = 4,  // deprecated
                i32 = 5,
            };

            constexpr size_t to_zigzag(auto rawn) {
                auto n = rawn < 0 ? -rawn : rawn;
                return n + n + (rawn < 0);
            }

        }  // namespace protobuf
    }      // namespace fnet
}  // namespace futils
