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
            template <class Bytes, class Next>
            Error read_0rtt_handshake_packet(Bytes&& b, tsize size, tsize* offset, FirstByte fb, Next&& next) {
                auto discard = [&](Error e, const char* where) {
                    next.packet_error(b, size, offset, fb, 1, e, where);
                    return e;
                };
                auto varint_error = [&](const char* where, varint::Error vierr) {
                    next.varint_decode_error(b, size, offset, fb, vierr, where);
                    if (vierr == varint::Error::need_more_length) {
                        return Error::not_enough_length;
                    }
                    return Error::decode_error;
                };

                tsize payload_length = 0, redsize = 0;
                varint::Error vierr = varint::decode(b, &payload_length, &redsize, size, *offset);
                if (vierr != varint::Error::none) {
                    return varint_error("read_0rtt_handshake_packet/payload_length", vierr);
                }
                tsize packet_number = 0;
                Error err = read_packet_number_long(b, size, offset, &fb, &packet_number, next, discard);
                if (err != Error::none) {
                    return err;
                }
                /* by here,
                   SrcConnectionID,
                   DstConnectionID,
                   packet_number
                   were saved at next
                */
                return next.long_h(b, size, offset, fb, payload_length);
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
