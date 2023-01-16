/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// length - length
#pragma once
#include "packet_head.h"

namespace utils {
    namespace quic {
        namespace packet {
            tsize length(Packet* p, bool no_packet_nubmer = false) {
                if (!p) {
                    return 0;
                }
                tsize res = 1;  // type
                if (p->flags.is_short()) {
                    auto s = static_cast<OneRTTPacket*>(p);
                    res += s->dstID_len;                     // dstID
                    res += s->flags.packet_number_length();  // packet_number
                    return res;
                }
                auto l = static_cast<LongPacket*>(p);
                res += 4;             // version
                res += 1;             // dstIDlen
                res += l->dstID_len;  // dstID
                res += 1;             // srcIDlen
                res += l->srcID_len;  // srcID
                if (p->flags.type() == types::Initial) {
                    auto i = static_cast<InitialPacket*>(p);
                    res += varint::least_enclen(i->token_len);
                    res += i->token_len;
                }
                if (!no_packet_nubmer && p->flags.type() != types::VersionNegotiation &&
                    p->flags.type() != types::Retry) {
                    res += varint::least_enclen(l->payload_length);  // length
                    res += l->flags.packet_number_length();          // packet_number
                }
                return res;
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils
