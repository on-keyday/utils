/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// version_negotiation - version negotiation packet
#pragma once
#include "packet_head.h"

namespace utils {
    namespace quic {
        namespace packet {
            template <class Next>
            Error read_version_negotiation_packet(byte* head, tsize size, tsize* offset, VersionNegotiationPacket& packet, Next&& next) {
                packet.raw_payload = head + *offset;
                packet.remain = size - *offset;
                packet.end_offset = *offset;
                // TODO(on-keyday): implement more!
                return next.version(&packet);
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
