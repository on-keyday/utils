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
            template <class Next>
            Error read_retry_packet(byte* head, tsize size, tsize* offset, RetryPacket& retry, Next&& next) {
                auto integity_offset = size - 16;
                if (*offset > integity_offset) {
                    next.packet_error(Error::not_enough_length, "read_retry_packet/integrity_tag", &retry);
                    return Error::not_enough_length;
                }
                auto integrity_tag = head + integity_offset;
                bytes::copy(retry.integrity_tag, integrity_tag, 16);
                retry.retry_token_len = integity_offset - *offset;
                retry.retry_token = head + *offset;
                retry.payload_length = 0;
                retry.remain = 0;
                retry.end_offset = size;
                return next.retry(&retry);
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
