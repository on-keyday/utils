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
#include "version_negotiation.h"
#include "../common/macros.h"

namespace utils {
    namespace quic {
        namespace packet {

            template <class Next>
            Error read_v1_short_packet_header(byte* head, tsize size, tsize* offset, FirstByte fb, Next&& next) {
                OneRTTPacket onertt{};
                onertt.raw_header = head;
                onertt.flags = fb;
                auto* list = next.get_issued_connid(head[*offset]);
                if (!list) {
                    next.packet_error(Error::unknown_connection_id, "read_v1_short_packet_header/dstID", &onertt);
                    return Error::unknown_connection_id;
                }
                bool ok = true;
                for (auto& v : *list) {
                    ok = true;
                    for (auto i = 0; i < v.size(); i++) {
                        if (head[*offset + i] != v[i]) {
                            ok = false;
                            break;
                        }
                    }
                    if (ok) {
                        onertt.dstID_len = v.size();
                        onertt.dstID = head + *offset;
                        *offset += v.size();
                        break;
                    }
                }
                if (!ok) {
                    next.packet_error(Error::unknown_connection_id, "read_v1_short_packet_header/dstID", &onertt);
                    return Error::unknown_connection_id;
                }
                onertt.raw_payload = head + *offset;
                onertt.remain = size - *offset;
                onertt.end_offset = *offset;
                onertt.payload_length = onertt.payload_length;
                return next.one_rtt(&onertt);
            }

            template <class Next>
            Error read_v1_long_packet_header(byte* head, tsize size, tsize* offset, FirstByte fb, uint version, Next&& next) {
                auto select_packet = [&](auto& long_packet, auto&& callback) {
                    // check version
                    if (version != 0 && !next.acceptable_version(version)) {
                        return next.discard(head, size, offset, fb, version, true);
                    }

                    auto discard = [&](Error e, const char* where) {
                        next.packet_error(e, where, &long_packet);
                        return e;
                    };
                    long_packet.raw_header = head;
                    long_packet.flags = fb;
                    long_packet.version = version;

                    // Destination Connection ID Length (8),
                    // Destination Connection ID (0..160),
                    auto err = read_connID(head, size, offset, long_packet.dstID, long_packet.dstID_len, next);
                    if (err != Error::none) {
                        return discard(err, "read_v1_long_packet_header/dstID");
                    }

                    // Source Connection ID Length(8),
                    // Source Connection ID (0..160),
                    err = read_connID(head, size, offset, long_packet.srcID, long_packet.srcID_len, next);
                    if (err != Error::none) {
                        return discard(err, "read_v1_long_packet_header/srcID");
                    }

                    auto res = callback();
                    next.put_bytes(std::move(long_packet.decrypted_payload));
                    return res;
                };

                // Type-Specific Payload (..),
                switch (fb.type()) {
                    case types::Initial: {
                        InitialPacket inip{};
                        return select_packet(inip, [&] {
                            return read_initial_packet(head, size, offset, inip, next);
                        });
                    }
                    case types::ZeroRTT: {
                        ZeroRTTPacket zerop{};
                        return select_packet(zerop, [&] {
                            return read_0rtt_packet(head, size, offset, zerop, next);
                        });
                    }
                    case types::HandShake: {
                        HandshakePacket handp{};
                        return select_packet(handp, [&]() {
                            return read_handshake_packet(head, size, offset, handp, next);
                        });
                    }
                    case types::Retry: {
                        RetryPacket retp{};
                        return select_packet(retp, [&] {
                            return read_retry_packet(head, size, offset, retp, next);
                        });
                    }
                    case types::VersionNegotiation: {
                        VersionNegotiationPacket verp{};
                        return select_packet(verp, [&] {
                            return read_version_negotiation_packet(head, size, offset, verp, next);
                        });
                    }
                    default:
                        unreachable("fb.type is masked with 0x3");
                }
            }

            template <PacketReadContext Next>
            Error read_packet(byte* b, tsize size, tsize* offset, Next&& next) {
                if (offset == nullptr || *offset >= size) {
                    return Error::invalid_argument;
                }
                FirstByte first_byte = guess_packet(b[*offset]);
                uint version = -1;
                bool versionSuc = false;
                first_byte.offset = *offset;
                ++*offset;
                auto is_short = first_byte.is_short();
                if (!is_short) {
                    // if first_byte.is_short() is false
                    // anyway the Version field exists
                    // Version (32),
                    versionSuc = read_version(b, &version, size, offset);
                }
                if (versionSuc && version == 0) {
                    // version negotiation packet!
                    // to detect byte as VersionNegotiation
                    // disable 0x40 bit
                    first_byte.raw &= ~0x40;
                    return read_v1_long_packet_header(b, size, offset, first_byte, version, next);
                }
                if (first_byte.invalid()) {
                    return next.discard(b, size, offset, first_byte, version, versionSuc);
                }
                if (!is_short) {
                    if (!versionSuc) {
                        // if version == 0 then packet may be VersionNegotiation packet
                        // see 17.2.1. Version Negotiation Packet
                        return next.discard(b, size, offset, first_byte, version, versionSuc);
                    }
                    return read_v1_long_packet_header(b, size, offset, first_byte, version, next);
                }
                if (is_short) {
                    return read_v1_short_packet_header(b, size, offset, first_byte, next);
                }
                return Error::invalid_fbinfo;
            }

        }  // namespace packet
    }      // namespace quic
}  // namespace utils
