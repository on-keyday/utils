/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// initial_packet - initial packet implementation
#pragma once

#include "packet_head.h"

namespace utils {
    namespace quic {
        namespace packet {
            // read_initial_packet reads initial packet fields subsequent the field Source Connection ID (0..160)
            template <class Next>
            Error read_initial_packet(byte* head, tsize size, tsize* offset, InitialPacket& packet, Next&& next) {
                tsize token_length = 0, redsize = 0;
                auto vierr = varint::decode(head, &token_length, &redsize, size, *offset);
                auto varint_error = [&](const char* where) {
                    next.varint_error(vierr, where, &packet);
                    if (vierr == varint::Error::need_more_length) {
                        return Error::not_enough_length;
                    }
                    return Error::decode_error;
                };
                if (vierr != varint::Error::none) {
                    return varint_error("read_initial_packet/token_length");
                }
                *offset += redsize;
                packet.token_len = token_length;
                auto old = *offset;
                auto discard = [&](Error e, const char* where) {
                    next.packet_error(e, where, &packet);
                    return e;
                };
                CHECK_OFFSET_CB(token_length, return discard(Error::not_enough_length, "read_initial_packet/token");)
                packet.token = head + *offset;
                *offset += token_length;
                redsize = 0;
                tsize payload_length = 0;
                vierr = varint::decode(head, &payload_length, &redsize, size, *offset);
                if (vierr != varint::Error::none) {
                    return varint_error("read_initial_packet/payload_length");
                }
                *offset += redsize;
                packet.payload_length = payload_length;
                packet.raw_payload = head + *offset;
                packet.remain = size - *offset;
                packet.end_offset = *offset;
                return next.initial(&packet);
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
