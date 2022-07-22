/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// 0_rtt_handshake - 0-rtt and handshake packet implementation
#pragma once

#include "packet_head.h"

namespace utils {
    namespace quic {
        namespace packet {
            template <class IPacket, class Next>
            Error read_0rtt_handshake_callback(byte* head, tsize size, tsize* offset, IPacket& packet, Next&& next,
                                               const char* where_length, auto&& callback) {
                auto varint_error = [&](const char* where, varint::Error vierr) {
                    next.varint_error(vierr, where, &packet);
                    if (vierr == varint::Error::need_more_length) {
                        return Error::not_enough_length;
                    }
                    return Error::decode_error;
                };
                tsize payload_length = 0, redsize = 0;
                varint::Error vierr = varint::decode(head, &payload_length, &redsize, size, *offset);
                if (vierr != varint::Error::none) {
                    return varint_error(where_length, vierr);
                }
                packet.payload_length = payload_length;
                packet.raw_payload = head + *offset;
                packet.remain = size - *offset;
                packet.end_offset = *offset;
                return callback();
            }

            template <class Next>
            Error read_handshake_packet(byte* head, tsize size, tsize* offset, HandshakePacket& packet, Next&& next) {
                return read_0rtt_handshake_callback(head, size, offset, packet, next, "read_handshake_packet/payload_length", [&] {
                    return next.handshake(&packet);
                });
            }

            template <class Next>
            Error read_0rtt_packet(byte* head, tsize size, tsize* offset, ZeroRTTPacket& packet, Next&& next) {
                return read_0rtt_handshake_callback(head, size, offset, packet, next, "read_0rtt_packet/payload_length", [&] {
                    return next.zero_rtt(&packet);
                });
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
