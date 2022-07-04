/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// retry - retry packet implementation
#pragma once
#include "packet_head.h"

namespace utils {
    namespace quic {
        namespace packet {
            // read_retry_packet reads retry packet
            // TODO: implement reading methods
            template <class Bytes, class Next>
            Error read_retry_packet(Bytes&& b, tsize size, tsize* offset, FirstByte fb, Next&& next) {
                /* by here,
                   SrcConnectionID,
                   DstConnectionID
                   were saved at next
                */
                return next.retry_h(b, size, offset, fb, size - *offset);
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
