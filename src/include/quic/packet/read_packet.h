/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// read_packet - packet read methods
#pragma once
#include "packet_head.h"
#include "initial.h"
#include "0_rtt_handshake.h"
#include "retry.h"
#include "../common/macros.h"

namespace utils {
    namespace quic {
        namespace packet {

            /*
                1-RTT Packet {
                    Header Form (1) = 0,
                    Fixed Bit (1) = 1,
                    Spin Bit (1),
                    Reserved Bits (2),
                    Key Phase (1),
                    Packet Number Length (2), // 1 byte
                    Destination Connection ID (0..160), // 0 ~ 20 byte (size known)
                    Packet Number (8..32), // 1 ~ 4 byte
                    Packet Payload (8..), // 1 byte ~
                }
            */
            template <class Bytes, class Next>
            constexpr Error read_v1_short_packet_header(Bytes&& b, tsize size, tsize* offset, FirstByte fb, Next&& next) {
                auto discard = [&](Error e, const char* where) {
                    next.packet_error(b, size, offset, fb, 1, e, where);
                    return e;
                };
                auto old = *offset;
                Error err = next.save_dst_known(b, size, offset);
                if (err != Error::none) {
                    return discard(err, "read_v1_short_packet_header/save_dst_known");
                }
                if (old > *offset) {
                    return discard(Error::consistency_error, "read_v1_short_packet_header/dstIDLen");
                }
                uint packet_number;
                err = read_packet_number_long(b, size, offset, &fb, &packet_number, next, discard);
                if (err != Error::none) {
                    return err;
                }
                /*
                    by here,
                    DstConnectionID,
                    packet_number
                    were saved at next
                */
                return next.short_h(b, size, offset, fb);
            }

            /*
                Long Header Packet {
                    Header Form (1) = 1,
                    Fixed Bit (1) = 1,
                    Long Packet Type (2),
                    Type-Specific Bits (4),  // 1 byte
                    Version (32),    // 4 byte
                    Destination Connection ID Length (8),
                    Destination Connection ID (0..160),
                    Source Connection ID Length (8),
                    Source Connection ID (0..160),
                    Type-Specific Payload (..),
                }
            */
            template <class Bytes, class Next>
            Error read_v1_long_packet_header(Bytes&& b, tsize size, tsize* offset, FirstByte fb, uint version, Next&& next) {
                // check version
                // if version is not 1
                // this implementation discard packet
                if (version != 1) {
                    return next.discard(b, size, offset, fb, version, true);
                }
                auto discard = [&](Error e, const char* where) {
                    next.packet_error(b, size, offset, fb, version, e, where);
                    return e;
                };

                if (*offset >= size) {
                    return discard(Error::not_enough_length, "read_v1_long_packet_header/dstIDLen");
                }

                // Destination Connection ID Length (8),
                byte dstIDLen = b[*offset];
                ++*offset;
                auto old = *offset;
                // Destination Connection ID (0..160),
                Error err = next.save_dst(b, size, offset, dstIDLen);
                if (err != Error::none) {
                    return discard(err, "read_v1_long_packet_header/save_dst");
                }
                if (old + dstIDLen != *offset) {
                    return discard(Error::consistency_error, "read_v1_long_packet_header/dstIDLen");
                }

                if (*offset >= size) {
                    return discard(Error::not_enough_length, "read_v1_long_packet_header/srcIDLen");
                }
                // Source Connection ID Length(8),
                byte srcIDLen = b[*offset];
                auto old = *offset;
                // Source Connection ID (0..160),
                err = next.save_src(b, size, offset, srcIDLen);
                if (err != Error::none) {
                    return discard(err, "read_v1_long_packet_header/save_src");
                }
                if (old + srcIDLen != *offset) {
                    return discard(Error::consistency_error, "read_v1_long_packet_header/srcIDLen");
                }

                // Type-Specific Payload (..),
                switch (fb.type) {
                    case types::Initial:
                        return read_initial_packet(b, size, offset, fb, next);
                    case types::ZeroRTT:
                        return read_0rtt_handshake_packet(b, size, offset, fb, next);
                    case types::HandShake:
                        return read_0rtt_handshake_packet(b, size, offset, fb, next);
                    case types::Retry:
                        return read_retry_packet(b, size, offset, fb, next);
                    default:
                        unreachable("fb.type is masked with 0x3");
                }
            }

            template <class Bytes, PacketHeadNext<Bytes> Next>
            Error read_packet_header(Bytes&& b, tsize size, tsize* offset, Next&& next) {
                if (offset == nullptr || *offset >= size) {
                    return Error::invalid_argument;
                }
                FirstByte first_byte = guess_packet(b[*offset]);
                uint version;
                bool versionSuc = false;
                first_byte.offset = *offset;
                ++*offset;
                if (first_byte.behave != FBInfo::short_header) {
                    // if first_byte.behave is not FBInfo::short_header
                    // anyway the Version field exists
                    // Version (32),
                    versionSuc = read_version(b, &version, size, offset);
                }
                if (first_byte.behave == FBInfo::should_discard) {
                    return next.discard(b, size, offset, first_byte, version, versionSuc);
                }
                if (first_byte.behave == FBInfo::long_header) {
                    if (!versionSuc || version == 0) {
                        // if version == 0 then packet may be VersionNegotiation packet
                        // see 17.2.1. Version Negotiation Packet
                        return next.discard(b, size, offset, first_byte, version, versionSuc);
                    }
                    return read_v1_long_packet_header(b, size, offset, first_byte, version, next);
                }
                if (first_byte.behave == FBInfo::short_header) {
                    return read_v1_short_packet_header(b, size, offset, first_byte, next);
                }
                return Error::invalid_fbinfo;
            }

        }  // namespace packet
    }      // namespace quic
}  // namespace utils
