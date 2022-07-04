/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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
            template <class Bytes, class Next>
            Error read_initial_packet(Bytes&& b, tsize size, tsize* offset, FirstByte fb, Next&& next) {
                tsize token_length = 0, redsize = 0;
                auto vierr = varint::decode(b, &token_length, &redsize, size, *offset);
                auto varint_error = [&](const char* where) {
                    next.varint_decode_error(b, size, offset, fb, vierr, where);
                    if (vierr == varint::Error::need_more_length) {
                        return Error::not_enough_length;
                    }
                    return Error::decode_error;
                };
                if (vierr != varint::Error::none) {
                    return varint_error("read_initial_packet/token_length");
                }
                *offset += redsize;
                auto old = *offset;
                auto discard = [&](Error e, const char* where) {
                    next.packet_error(b, size, offset, fb, 1, e, where);
                    return e;
                };
                Error err = next.save_initial_token(b, size, offset, token_length);
                if (err != Error::none) {
                    return discard(err, "read_initial_packet/save_initial_token");
                }
                if (old + token_length != *offset) {
                    return discard(Error::consistency_error, "read_initial_packet/initial_token");
                }
                redsize = 0;
                tsize payload_length = 0;
                vierr = varint::decode(b, &payload_length, &redsize, size, *offset);
                if (vierr != varint::Error::none) {
                    return varint_error("read_initial_packet/payload_length");
                }
                tsize packet_number = 0;
                err = read_packet_number_long(b, size, offset, &fb, &packet_number, next, discard);
                if (err != Error::none) {
                    return err;
                }
                /* by here,
                   SrcConnectionID,
                   DstConnectionID,
                   initial_token,
                   packet_number
                   were saved at next
                */
                return next.long_h(b, size, offset, fb, payload_length);
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
